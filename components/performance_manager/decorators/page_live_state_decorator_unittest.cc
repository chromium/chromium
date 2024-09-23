// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"

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
    kOnIsConnectedToHidDeviceChanged,
    kOnIsConnectedToSerialPortChanged,
    kOnIsCapturingVideoChanged,
    kOnIsCapturingAudioChanged,
    kOnIsBeingMirroredChanged,
    kOnIsCapturingWindowChanged,
    kOnIsCapturingDisplayChanged,
    kOnIsAutoDiscardableChanged,
    kOnWasDiscardedChanged,
    kOnIsActiveTabChanged,
    kOnIsPinnedTabChanged,
    kOnIsDevToolsOpenChanged,
    kOnAccessibilityModeChanged,
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
  void OnIsConnectedToHidDeviceChanged(const PageNode* page_node) override {
    latest_function_called_ =
        ObserverFunction::kOnIsConnectedToHidDeviceChanged;
    page_node_passed_ = page_node;
  }
  void OnIsConnectedToSerialPortChanged(const PageNode* page_node) override {
    latest_function_called_ =
        ObserverFunction::kOnIsConnectedToSerialPortChanged;
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
  void OnIsActiveTabChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsActiveTabChanged;
    page_node_passed_ = page_node;
  }
  void OnIsPinnedTabChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsPinnedTabChanged;
    page_node_passed_ = page_node;
  }
  void OnIsDevToolsOpenChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsDevToolsOpenChanged;
    page_node_passed_ = page_node;
  }
  void OnAccessibilityModeChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnAccessibilityModeChanged;
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

  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>());
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return content::GetUIThreadTaskRunner({});
  }

 private:
  std::unique_ptr<TestPageLiveStateObserver> observer_;
};

TEST_F(PageLiveStateDecoratorTest, Usb) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnDeviceConnectionTypesChanged(
        contents, content::WebContentsObserver::DeviceConnectionType::kUSB,
        value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToUSBDevice, setter);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Bluetooth) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnDeviceConnectionTypesChanged(
        contents,
        content::WebContentsObserver::DeviceConnectionType::kBluetooth, value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, setter);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Hid) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnDeviceConnectionTypesChanged(
        contents, content::WebContentsObserver::DeviceConnectionType::kHID,
        value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToHidDevice, setter);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToHidDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Serial) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnDeviceConnectionTypesChanged(
        contents, content::WebContentsObserver::DeviceConnectionType::kSerial,
        value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToSerialPort, setter);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::
          kOnIsConnectedToSerialPortChanged);
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

TEST_F(PageLiveStateDecoratorTest, OnIsActiveTabChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab,
      &PageLiveStateDecorator::SetIsActiveTab,
      /*default_state=*/false);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsActiveTabChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsPinnedTabChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab,
      &PageLiveStateDecorator::SetIsPinnedTab,
      /*default_state=*/false);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsPinnedTabChanged);
}

// DevTools not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageLiveStateDecoratorTest, OnIsDevToolsOpenChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsDevToolsOpen,
      &PageLiveStateDecorator::SetIsDevToolsOpen,
      /*default_state=*/false);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsDevToolsOpenChanged);
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PageLiveStateDecoratorTest, OnAccessibilityModeChanged) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  const auto expect_mode_on_graph = [&node](ui::AXMode accessibility_mode) {
    RunInGraph([&node, accessibility_mode]() {
      ASSERT_TRUE(node);
      auto* data =
          PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());
      ASSERT_TRUE(data);
      EXPECT_EQ(data->GetAccessibilityMode(), accessibility_mode);
    });
  };

  // All mode flags off by default.
  ASSERT_NO_FATAL_FAILURE(expect_mode_on_graph(ui::AXMode()));

  // Pretend that the property changed and make sure that the PageNode data gets
  // updated.
  PageLiveStateDecorator::SetAccessibilityMode(web_contents(),
                                               ui::kAXModeComplete);
  ASSERT_NO_FATAL_FAILURE(expect_mode_on_graph(ui::kAXModeComplete));

  // Switch back to the default state.
  PageLiveStateDecorator::SetAccessibilityMode(web_contents(), ui::AXMode());
  ASSERT_NO_FATAL_FAILURE(expect_mode_on_graph(ui::AXMode()));

  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnAccessibilityModeChanged);
}

TEST_F(PageLiveStateDecoratorTest, UpdateTitleInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(node);
        auto* node_impl = PageNodeImpl::FromNode(node.get());
        auto* data =
            PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

        // Updating the title while the node is visible does nothing.
        node_impl->SetIsVisible(true);
        node_impl->OnTitleUpdated();
        EXPECT_EQ(data->UpdatedTitleOrFaviconInBackground(), false);

        node_impl->SetIsVisible(false);
        node_impl->OnTitleUpdated();
        EXPECT_EQ(data->UpdatedTitleOrFaviconInBackground(), true);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PageLiveStateDecoratorTest, UpdateFaviconInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(node);
        auto* node_impl = PageNodeImpl::FromNode(node.get());
        auto* data =
            PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

        // Updating the favicon while the node is visible does nothing.
        node_impl->SetIsVisible(true);
        node_impl->OnFaviconUpdated();
        EXPECT_EQ(data->UpdatedTitleOrFaviconInBackground(), false);

        node_impl->SetIsVisible(false);
        node_impl->OnFaviconUpdated();
        EXPECT_EQ(data->UpdatedTitleOrFaviconInBackground(), true);

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace performance_manager
