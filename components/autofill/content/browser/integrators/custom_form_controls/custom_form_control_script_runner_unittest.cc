// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

// Fakes the renderer's agent of a frame: records the scripts loaded into it.
class FakeCustomFormControlAgent : public mojom::CustomFormControlAgent {
 public:
  // Binds `rfh`'s requests for the agent to this while this exists.
  explicit FakeCustomFormControlAgent(content::RenderFrameHost* rfh)
      : rfh_(CHECK_DEREF(rfh)) {
    rfh_->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::CustomFormControlAgent::Name_,
        base::BindRepeating(&FakeCustomFormControlAgent::Bind,
                            base::Unretained(this)));
  }
  ~FakeCustomFormControlAgent() override {
    rfh_->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::CustomFormControlAgent::Name_, base::NullCallback());
  }

  // The documents and scripts of the `LoadUnsafeScript()` calls, in call order.
  const std::vector<std::pair<blink::DocumentToken, std::string>>&
  loaded_scripts() const {
    return loaded_scripts_;
  }

  // mojom::CustomFormControlAgent:
  void LoadUnsafeScript(const blink::DocumentToken& document,
                        const std::string& script,
                        LoadUnsafeScriptCallback callback) override {
    loaded_scripts_.emplace_back(document, script);
    std::move(callback).Run(/*success=*/true);
  }

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<mojom::CustomFormControlAgent>(
                  std::move(handle)));
  }

  const raw_ref<content::RenderFrameHost> rfh_;
  mojo::AssociatedReceiverSet<mojom::CustomFormControlAgent> receivers_;
  std::vector<std::pair<blink::DocumentToken, std::string>> loaded_scripts_;
};

class CustomFormControlScriptRunnerTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    runner_ =
        std::make_unique<CustomFormControlScriptRunner>(client(), &index_);
    runner_->set_script_found_callback_for_testing(
        base::BindLambdaForTesting([this](const std::string& script) {
          found_scripts_.push_back(script);
        }));
  }

  void TearDown() override {
    runner_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Initializes `index_` with a single entry that maps `host` to `script` and
  // waits until it is done. Returns false if that fails.
  [[nodiscard]] bool InitializeIndex(std::string_view host,
                                     std::string_view script) {
    const base::FilePath dir = temp_dir_.GetPath();
    if (!base::WriteFile(
            dir.AppendASCII("index.json"),
            base::StrCat({R"([{"host": ")", host,
                          R"(", "script_path": "script.js"}])"})) ||
        !base::WriteFile(dir.AppendASCII("script.js"), script)) {
      return false;
    }
    index_.InitializeFromDirectory(dir);
    return base::test::RunUntil([&] { return index_.is_initialized(); });
  }

  // Notifies the runner that forms were seen in the main frame.
  void SeeForms() {
    runner().OnAfterFormsSeen(manager(), {test::MakeFormGlobalId()}, {});
  }

  CustomFormControlScriptRunner& runner() { return *runner_; }

  TestContentAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& manager() {
    return *autofill_manager_injector_[main_rfh()];
  }

  const std::vector<std::string>& found_scripts() const {
    return found_scripts_;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  base::ScopedTempDir temp_dir_;
  CustomFormControlScriptIndex index_;
  std::unique_ptr<CustomFormControlScriptRunner> runner_;
  std::vector<std::string> found_scripts_;
};

// Tests that the script for the frame's URL is found when forms are seen.
TEST_F(CustomFormControlScriptRunnerTest, FindsScriptWhenFormsAreSeen) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

// Tests that each document is only looked up once.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpEachDocumentOnce) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

// Tests that the document a navigation commits is looked up again.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpNewDocument) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  NavigateAndCommit(GURL("https://example.com/payment"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script", "example script"));
}

// Tests that a frame is looked up again after its manager is reset for a new
// document in the same frame.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpAgainAfterReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script", "example script"));
}

// Tests that a found script is loaded into the frame's renderer for the
// frame's current document.
TEST_F(CustomFormControlScriptRunnerTest, LoadsScriptIntoRenderer) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  FakeCustomFormControlAgent agent(main_rfh());
  SeeForms();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !agent.loaded_scripts().empty(); }));
  EXPECT_THAT(
      agent.loaded_scripts(),
      ElementsAre(Pair(main_rfh()->GetDocumentToken(), "example script")));
}

// Tests that no script is found for URLs without one.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresUrlsWithoutScript) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://other.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), IsEmpty());
}

// Tests that forms seen before the index is initialized don't count as a
// lookup, so that forms seen later in the same document are looked up.
TEST_F(CustomFormControlScriptRunnerTest, WaitsForIndex) {
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), IsEmpty());
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

}  // namespace
}  // namespace autofill
