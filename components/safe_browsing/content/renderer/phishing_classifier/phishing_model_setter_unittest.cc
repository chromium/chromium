// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the safe browsing phishing model setter.
//
// Unfortunately, the current implementation is rather difficult to test (it's
// highly encapsulated, so there are few observable effects to probe).
//
// The mojom file implementing all the interfaces being used here is
// components/safe_browsing/content/common/safe_browsing.mojom

#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_model_setter_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace safe_browsing {
namespace {

// Implementation of the `PhishingModelSetterTestObserver` mojom interface
// that counts the number of `PhishingModelUpdated` messages it receives,
// and lets tests wait for the next notification.
class FakeTestObserver : public mojom::PhishingModelSetterTestObserver {
 public:
  mojo::PendingRemote<mojom::PhishingModelSetterTestObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForNotification() {
    base::RunLoop run_loop;
    on_update_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int update_count() const { return update_count_; }

 private:
  void PhishingModelUpdated() override {
    ++update_count_;
    if (on_update_) {
      std::move(on_update_).Run();
    }
  }

  int update_count_ = 0;
  base::OnceClosure on_update_;
  mojo::Receiver<mojom::PhishingModelSetterTestObserver> receiver_{this};
};

// Waits for ScorerStorage to fire OnScorerChanged() once.
class ScorerChangeWaiter : public ScorerStorage::Observer {
 public:
  ScorerChangeWaiter() { ScorerStorage::GetInstance()->AddObserver(this); }
  ~ScorerChangeWaiter() override {
    ScorerStorage::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  void OnScorerChanged() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

// Test fixture that allows users to create new `AssociatedRemote`s for the
// `PhishingModelSetter` interface.
class PhishingModelSetterTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeatureState(kRustPhishingModelSetter, GetParam());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    static_cast<content::RenderThreadObserver&>(setter_).RegisterMojoInterfaces(
        &registry_);
  }

  void TearDown() override {
    // Let any pending disconnect notifications run. Rust receivers own
    // themselves until they see the disconnect, so skipping this would leak
    // them.
    task_environment_.RunUntilIdle();
    static_cast<content::RenderThreadObserver&>(setter_)
        .UnregisterMojoInterfaces(&registry_);
    mojo::AssociatedRemote<mojom::PhishingModelSetter> remote;
    EXPECT_FALSE(TryBindNewRemote(remote));
    ScorerStorage::GetInstance()->ClearScorer();
  }

  // Asks `registry_` to bind `remote`, matching how ClientSideDetectionService
  // connects on each model push. Returns false if the interface is not
  // registered.
  bool TryBindNewRemote(
      mojo::AssociatedRemote<mojom::PhishingModelSetter>& remote) {
    mojo::ScopedInterfaceEndpointHandle handle =
        remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle();
    return registry_.TryBindInterface(mojom::PhishingModelSetter::Name_,
                                      &handle);
  }

  mojo::AssociatedRemote<mojom::PhishingModelSetter> BindNewRemote() {
    mojo::AssociatedRemote<mojom::PhishingModelSetter> remote;
    EXPECT_TRUE(TryBindNewRemote(remote));
    return remote;
  }

  // Calls SetTestObserver on `remote` and waits for its acknowledgement reply.
  void SetTestObserverAndWait(
      mojo::AssociatedRemote<mojom::PhishingModelSetter>& remote,
      mojo::PendingRemote<mojom::PhishingModelSetterTestObserver> observer) {
    base::RunLoop run_loop;
    remote->SetTestObserver(std::move(observer), run_loop.QuitClosure());
    run_loop.Run();
  }

  // Dispatches every message sent so far, including any test observer
  // notifications they trigger: the setter, its receivers and this test all
  // share one sequence, so running it until idle leaves nothing in flight.
  // (`AssociatedRemote::FlushForTesting()` is deliberately not used because
  // the Rust implementation does not yet answer the control message it relies
  // on.)
  void DispatchPendingMessages() { task_environment_.RunUntilIdle(); }

  // Creates a temporary file with `content` and opens it read-only, as
  // required by the mojom ReadOnlyFile serializer.
  base::File CreateReadOnlyModelFile(
      const char* name,
      std::string_view content = "not a real tflite model") {
    base::FilePath path = temp_dir_.GetPath().AppendASCII(name);
    EXPECT_TRUE(base::WriteFile(path, content));
    return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  Scorer* scorer() { return ScorerStorage::GetInstance()->GetScorer(); }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  PhishingModelSetterImpl setter_;
  blink::AssociatedInterfaceRegistry registry_;
};

// This lets us test the phishing model setter with both the Rust and
// original C++ implementations.
INSTANTIATE_TEST_SUITE_P(All, PhishingModelSetterTest, testing::Bool());

TEST_P(PhishingModelSetterTest, ModelLifecycleAndObserverNotifications) {
  FakeTestObserver observer;
  auto remote = BindNewRemote();
  SetTestObserverAndWait(remote, observer.BindNewPipeAndPassRemote());

  // Passing a 0-byte file makes Scorer::Create fail, so nothing is installed
  // and the test observer is not notified.
  remote->SetPhishingTfLiteModel(10, 20,
                                 CreateReadOnlyModelFile("empty.tflite", ""));
  DispatchPendingMessages();
  EXPECT_EQ(scorer(), nullptr);
  EXPECT_EQ(observer.update_count(), 0);

  // Passing no file at all is allowed and installs a (model-less) Scorer,
  // which notifies the test observer.
  remote->SetPhishingTfLiteModel(10, 20, base::File());
  observer.WaitForNotification();
  ASSERT_NE(scorer(), nullptr);
  EXPECT_EQ(observer.update_count(), 1);

  // Attaching an embedding model updates the existing Scorer in place rather
  // than installing a new one, so the test observer is not notified.
  Scorer* const installed_scorer = scorer();
  remote->AttachImageEmbeddingModelAndDimensions(
      30, 40, CreateReadOnlyModelFile("embedding.tflite"));
  DispatchPendingMessages();
  EXPECT_EQ(scorer(), installed_scorer);
  EXPECT_EQ(observer.update_count(), 1);

  // ClearScorer removes the Scorer, again without notifying the test observer.
  {
    ScorerChangeWaiter change_waiter;
    remote->ClearScorer();
    change_waiter.Wait();
  }
  EXPECT_EQ(scorer(), nullptr);
  EXPECT_EQ(observer.update_count(), 1);

  // Setting both models from non-empty files installs a new Scorer and
  // notifies the test observer a second time.
  remote->SetImageEmbeddingAndPhishingTfLiteModel(
      10, 20, CreateReadOnlyModelFile("visual.tflite"), 30, 40,
      CreateReadOnlyModelFile("embedding2.tflite"));
  observer.WaitForNotification();
  EXPECT_NE(scorer(), nullptr);
  EXPECT_EQ(observer.update_count(), 2);
}

// Verifies that a test observer registered on one Mojo connection still
// receives notifications when the browser opens a new connection to push a
// model.
TEST_P(PhishingModelSetterTest, TestObserverSurvivesRebinding) {
  FakeTestObserver observer;

  // Register the test observer on a first connection, then close it.
  {
    auto first_remote = BindNewRemote();
    SetTestObserverAndWait(first_remote, observer.BindNewPipeAndPassRemote());
  }

  // Open a second connection and set a model; the observer from the first
  // connection should still be notified.
  auto second_remote = BindNewRemote();
  second_remote->SetPhishingTfLiteModel(
      10, 20, CreateReadOnlyModelFile("visual.tflite"));
  observer.WaitForNotification();
  EXPECT_NE(scorer(), nullptr);
  EXPECT_EQ(observer.update_count(), 1);
}

}  // namespace
}  // namespace safe_browsing
