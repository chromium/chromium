// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// A test version of a PageLiveStateObserver that records the latest function
// that has been called. Gmock isn't used here as instances of this class will
// be used on a different sequence than the main one and that this add a lot of
// extra complexity.
class TestPageLiveStateObserver : public PageLiveStateObserver {
 public:
  TestPageLiveStateObserver() = default;
  ~TestPageLiveStateObserver() override = default;
  TestPageLiveStateObserver(const TestPageLiveStateObserver& other) = delete;
  TestPageLiveStateObserver& operator=(const TestPageLiveStateObserver&) =
      delete;

  enum class ObserverFunction {
    kNone,
    kOnIsConnectedToUSBDeviceChanged,
    kOnIsConnectedToBluetoothDeviceChanged,
    kOnIsCapturingVideoChanged,
    kOnIsCapturingAudioChanged,
    kOnIsBeingMirroredChanged,
    kOnIsCapturingWindowChanged,
    kOnIsCapturingDisplayChanged,
    kOnIsAutoDiscardableChanged,
    kOnWasDiscardedChanged,
  };

  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override {
    latest_function_called_ =
        ObserverFunction::kOnIsConnectedToUSBDeviceChanged;
    page_node_passed_ = page_node;
  }
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override {
    latest_function_called_ =
        ObserverFunction::kOnIsConnectedToBluetoothDeviceChanged;
    page_node_passed_ = page_node;
  }
  void OnIsCapturingVideoChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsCapturingVideoChanged;
    page_node_passed_ = page_node;
  }
  void OnIsCapturingAudioChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsCapturingAudioChanged;
    page_node_passed_ = page_node;
  }
  void OnIsBeingMirroredChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsBeingMirroredChanged;
    page_node_passed_ = page_node;
  }
  void OnIsCapturingWindowChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsCapturingWindowChanged;
    page_node_passed_ = page_node;
  }
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsCapturingDisplayChanged;
    page_node_passed_ = page_node;
  }
  void OnIsAutoDiscardableChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsAutoDiscardableChanged;
    page_node_passed_ = page_node;
  }
  void OnWasDiscardedChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnWasDiscardedChanged;
    page_node_passed_ = page_node;
  }

  ObserverFunction latest_function_called_ = ObserverFunction::kNone;
  raw_ptr<const PageNode> page_node_passed_ = nullptr;
};

}  // namespace

class PageLiveStateDecoratorTest : public PerformanceManagerTestHarness {
 protected:
  PageLiveStateDecoratorTest() = default;
  ~PageLiveStateDecoratorTest() override = default;
  PageLiveStateDecoratorTest(const PageLiveStateDecoratorTest& other) = delete;
  PageLiveStateDecoratorTest& operator=(const PageLiveStateDecoratorTest&) =
      delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    observer_ = std::make_unique<TestPageLiveStateObserver>();

    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<PageNode> page_node,
               TestPageLiveStateObserver* observer,
               base::OnceClosure quit_closure) {
              EXPECT_TRUE(page_node);
              PageLiveStateDecorator::Data::GetOrCreateForPageNode(
                  page_node.get())
                  ->AddObserver(observer);
              std::move(quit_closure).Run();
            },
            PerformanceManager::GetPrimaryPageNodeForWebContents(
                web_contents()),
            observer_.get(), std::move(quit_closure)));
    run_loop.Run();
  }

  void TearDown() override {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<PageNode> page_node,
               TestPageLiveStateObserver* observer,
               base::OnceClosure quit_closure) {
              EXPECT_TRUE(page_node);
              PageLiveStateDecorator::Data::GetOrCreateForPageNode(
                  page_node.get())
                  ->RemoveObserver(observer);
              std::move(quit_closure).Run();
            },
            PerformanceManager::GetPrimaryPageNodeForWebContents(
                web_contents()),
            observer_.get(), std::move(quit_closure)));
    run_loop.Run();

    PerformanceManager::GetTaskRunner()->DeleteSoon(FROM_HERE,
                                                    std::move(observer_));
    DeleteContents();
    PerformanceManagerTestHarness::TearDown();
  }

  void VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction expected_call) {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<PageNode> page_node,
               TestPageLiveStateObserver* observer,
               TestPageLiveStateObserver::ObserverFunction expected_call,
               base::OnceClosure quit_closure) {
              EXPECT_TRUE(page_node);
              EXPECT_EQ(expected_call, observer->latest_function_called_);
              EXPECT_EQ(page_node.get(), observer->page_node_passed_);
              std::move(quit_closure).Run();
            },
            PerformanceManager::GetPrimaryPageNodeForWebContents(
                web_contents()),
            observer_.get(), expected_call, std::move(quit_closure)));
    run_loop.Run();
  }

 private:
  std::unique_ptr<TestPageLiveStateObserver> observer_;
};

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToUSBDeviceChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToUSBDevice,
      &PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToBluetoothDeviceChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice,
      &PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingVideoChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingVideo,
      &PageLiveStateDecorator::OnIsCapturingVideoChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingVideoChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingAudioChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingAudio,
      &PageLiveStateDecorator::OnIsCapturingAudioChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingAudioChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsBeingMirroredChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsBeingMirrored,
      &PageLiveStateDecorator::OnIsBeingMirroredChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsBeingMirroredChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingWindowChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingWindow,
      &PageLiveStateDecorator::OnIsCapturingWindowChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingWindowChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingDisplayChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingDisplay,
      &PageLiveStateDecorator::OnIsCapturingDisplayChanged);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsCapturingDisplayChanged);
}

TEST_F(PageLiveStateDecoratorTest, SetIsAutoDiscardable) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsAutoDiscardable,
      &PageLiveStateDecorator::SetIsAutoDiscardable,
      /*default_state=*/true);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsAutoDiscardableChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnWasDiscardedChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::WasDiscarded,
      &PageLiveStateDecorator::SetWasDiscarded,
      /*default_state=*/false);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnWasDiscardedChanged);
}

}  // namespace performance_manager
