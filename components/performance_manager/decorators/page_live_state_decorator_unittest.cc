// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_capability_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// A test version of a PageLiveStateObserver that records the latest function
// that has been called.
class TestPageLiveStateObserver : public PageLiveStateObserver {
 public:
  explicit TestPageLiveStateObserver(const PageNode* page_node) {
    scoped_observation_.Observe(
        PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node));
  }

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
    kOnIsActiveTabChanged,
    kOnIsPinnedTabChanged,
    kOnIsDevToolsOpenChanged,
    kOnUpdatedTitleOrFaviconInBackgroundChanged,
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
  void OnUpdatedTitleOrFaviconInBackgroundChanged(
      const PageNode* page_node) override {
    latest_function_called_ =
        ObserverFunction::kOnUpdatedTitleOrFaviconInBackgroundChanged;
    page_node_passed_ = page_node;
  }

  ObserverFunction latest_function_called() const {
    return latest_function_called_;
  }

  const PageNode* page_node_passed() const { return page_node_passed_; }

 private:
  ObserverFunction latest_function_called_ = ObserverFunction::kNone;
  raw_ptr<const PageNode> page_node_passed_ = nullptr;
  base::ScopedObservation<PageLiveStateDecorator::Data, PageLiveStateObserver>
      scoped_observation_{this};
};

class MockPageLiveStateObserver : public PageLiveStateObserver {
 public:
  MOCK_METHOD(void,
              OnIsConnectedToUSBDeviceChanged,
              (const PageNode* page_node),
              (override));
};

}  // namespace

class PageLiveStateDecoratorTest : public PerformanceManagerTestHarness {
 public:
  PageLiveStateDecoratorTest() = default;
  ~PageLiveStateDecoratorTest() override = default;

  PageLiveStateDecoratorTest(const PageLiveStateDecoratorTest& other) = delete;
  PageLiveStateDecoratorTest& operator=(const PageLiveStateDecoratorTest&) =
      delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());

    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
    ASSERT_TRUE(page_node);
    observer_.emplace(page_node.get());
  }

  void TearDown() override {
    DeleteObservedWebContents();
    PerformanceManagerTestHarness::TearDown();
  }

  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>());
    PerformanceManagerTestHarness::OnGraphCreated(graph);
  }

 protected:
  void VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction expected_call) {
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
    ASSERT_TRUE(page_node);
    ASSERT_TRUE(observer_);
    EXPECT_EQ(expected_call, observer_->latest_function_called());
    EXPECT_EQ(page_node.get(), observer_->page_node_passed());
  }

  void DeleteObservedWebContents() {
    // Stop observing the WebContents that was created in SetUp before deleting
    // it.
    observer_.reset();

    DeleteContents();

    // The inherited TearDown() method uses RunUntilIdle to wait for any tasks
    // posted during WebContents destruction to run. RunUntilIdle is flaky so
    // use RunUntilQuit instead: wait for a task posted after anything queued by
    // DeleteContents() to run before continuing.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();
  }

 private:
  std::optional<TestPageLiveStateObserver> observer_;
};

TEST_F(PageLiveStateDecoratorTest, Usb) {
  EXPECT_FALSE(PageLiveStateDecorator::IsConnectedToUSBDevice(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kUSB, value);
    EXPECT_EQ(PageLiveStateDecorator::IsConnectedToUSBDevice(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToUSBDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Bluetooth) {
  EXPECT_FALSE(
      PageLiveStateDecorator::IsConnectedToBluetoothDevice(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kBluetoothConnected,
        value);
    EXPECT_EQ(PageLiveStateDecorator::IsConnectedToBluetoothDevice(contents),
              value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Hid) {
  EXPECT_FALSE(PageLiveStateDecorator::IsConnectedToHidDevice(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kHID, value);
    EXPECT_EQ(PageLiveStateDecorator::IsConnectedToHidDevice(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToHidDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToHidDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Serial) {
  EXPECT_FALSE(PageLiveStateDecorator::IsConnectedToSerialPort(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kSerial, value);
    EXPECT_EQ(PageLiveStateDecorator::IsConnectedToSerialPort(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToSerialPort, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToSerialPortChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingVideoChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsCapturingVideo(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnIsCapturingVideoChanged(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsCapturingVideo(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingVideo, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingVideoChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingAudioChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsCapturingAudio(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnIsCapturingAudioChanged(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsCapturingAudio(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingAudio, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingAudioChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsBeingMirroredChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsBeingMirrored(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnIsBeingMirroredChanged(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsBeingMirrored(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsBeingMirrored, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsBeingMirroredChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingWindowChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsCapturingWindow(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnIsCapturingWindowChanged(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsCapturingWindow(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingWindow, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingWindowChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingDisplayChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsCapturingDisplay(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnIsCapturingDisplayChanged(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsCapturingDisplay(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingDisplay, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsCapturingDisplayChanged);
}

TEST_F(PageLiveStateDecoratorTest, SetIsDiscarded) {
  EXPECT_FALSE(PageLiveStateDecorator::IsDiscarded(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::SetIsDiscarded(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsDiscarded(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsDiscarded, setter,
      /*default_state=*/false);
}

TEST_F(PageLiveStateDecoratorTest, SetIsAutoDiscardable) {
  EXPECT_TRUE(PageLiveStateDecorator::IsAutoDiscardable(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::SetIsAutoDiscardable(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsAutoDiscardable(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsAutoDiscardable, setter,
      /*default_state=*/true);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsAutoDiscardableChanged);
}

TEST_F(PageLiveStateDecoratorTest, AutoDiscardablePersistsThroughDiscard) {
  PageLiveStateDecorator::SetIsAutoDiscardable(web_contents(), false);

  // Simulate the tab containing web_contents() being discarded by an
  // extension and replaced by a placeholder.
  auto placeholder_contents = CreateTestWebContents();
  auto placeholder_page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          placeholder_contents.get());
  ASSERT_TRUE(placeholder_page_node);
  EXPECT_TRUE(
      PageLiveStateDecorator::IsAutoDiscardable(placeholder_contents.get()));

  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);
  auto* node_impl = PageNodeImpl::FromNode(node.get());
  node_impl->OnAboutToBeDiscarded(/*new_page_node=*/placeholder_page_node);

  EXPECT_FALSE(
      PageLiveStateDecorator::IsAutoDiscardable(placeholder_contents.get()));
}

TEST_F(PageLiveStateDecoratorTest, OnIsActiveTabChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsActiveTab(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::SetIsActiveTab(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsActiveTab(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsActiveTabChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsPinnedTabChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsPinnedTab(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::SetIsPinnedTab(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsPinnedTab(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsPinnedTabChanged);
}

// DevTools not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageLiveStateDecoratorTest, OnIsDevToolsOpenChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::IsDevToolsOpen(web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::SetIsDevToolsOpen(contents, value);
    EXPECT_EQ(PageLiveStateDecorator::IsDevToolsOpen(contents), value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsDevToolsOpen, setter);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsDevToolsOpenChanged);
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PageLiveStateDecoratorTest, OnUpdatedTitleOrFaviconInBackgroundChanged) {
  EXPECT_FALSE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::Data::GetOrCreateForPageNode(
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents).get())
        ->SetUpdatedTitleOrFaviconInBackgroundForTesting(value);
    EXPECT_EQ(
        PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(contents),
        value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::UpdatedTitleOrFaviconInBackground, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnUpdatedTitleOrFaviconInBackgroundChanged);
}

TEST_F(PageLiveStateDecoratorTest, UpdateTitleInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);
  auto* node_impl = PageNodeImpl::FromNode(node.get());
  auto* data = PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

  EXPECT_FALSE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));

  // Updating the title while the node is visible does nothing.
  node_impl->SetIsVisible(true);
  node_impl->OnTitleUpdated();
  EXPECT_FALSE(data->UpdatedTitleOrFaviconInBackground());
  EXPECT_FALSE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));

  node_impl->SetIsVisible(false);
  node_impl->OnTitleUpdated();
  EXPECT_TRUE(data->UpdatedTitleOrFaviconInBackground());
  EXPECT_TRUE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));
}

TEST_F(PageLiveStateDecoratorTest, UpdateFaviconInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);
  auto* node_impl = PageNodeImpl::FromNode(node.get());
  auto* data = PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

  EXPECT_FALSE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));

  // Updating the favicon while the node is visible does nothing.
  node_impl->SetIsVisible(true);
  node_impl->OnFaviconUpdated();
  EXPECT_FALSE(data->UpdatedTitleOrFaviconInBackground());
  EXPECT_FALSE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));

  node_impl->SetIsVisible(false);
  node_impl->OnFaviconUpdated();
  EXPECT_TRUE(data->UpdatedTitleOrFaviconInBackground());
  EXPECT_TRUE(PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
      web_contents()));
}

TEST_F(PageLiveStateDecoratorTest, AllPageObservers) {
  ::testing::StrictMock<MockPageLiveStateObserver> observer1;
  ::testing::StrictMock<MockPageLiveStateObserver> observer2;
  EXPECT_FALSE(PageLiveStateDecorator::HasAllPageObserver(&observer1));
  EXPECT_FALSE(PageLiveStateDecorator::HasAllPageObserver(&observer2));

  base::WeakPtr<PageNode> page1 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(page1);
  PageLiveStateDecorator::AddAllPageObserver(&observer1);
  EXPECT_TRUE(PageLiveStateDecorator::HasAllPageObserver(&observer1));

  // `observer1` should be notified about change to `page1`.
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page1.get()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page1.get())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  ::testing::Mock::VerifyAndClear(&observer1);

  auto contents2 = CreateTestWebContents();
  auto page2 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents2.get());
  ASSERT_TRUE(page2);

  // `observer1` should be notified about changes to both pages.
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page1.get()));
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page2.get()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page1.get())
      ->SetIsConnectedToUSBDeviceForTesting(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page2.get())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  ::testing::Mock::VerifyAndClear(&observer1);

  PageLiveStateDecorator::AddAllPageObserver(&observer2);
  EXPECT_TRUE(PageLiveStateDecorator::HasAllPageObserver(&observer2));

  // Both observers should be notified about changes to both pages.
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page1.get()));
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page2.get()));
  EXPECT_CALL(observer2, OnIsConnectedToUSBDeviceChanged(page1.get()));
  EXPECT_CALL(observer2, OnIsConnectedToUSBDeviceChanged(page2.get()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page1.get())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page2.get())
      ->SetIsConnectedToUSBDeviceForTesting(false);
  ::testing::Mock::VerifyAndClear(&observer1);
  ::testing::Mock::VerifyAndClear(&observer2);

  DeleteObservedWebContents();
  EXPECT_FALSE(page1);

  // Both observers should be notified about changes to `page2`.
  EXPECT_CALL(observer1, OnIsConnectedToUSBDeviceChanged(page2.get()));
  EXPECT_CALL(observer2, OnIsConnectedToUSBDeviceChanged(page2.get()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page2.get())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  ::testing::Mock::VerifyAndClear(&observer1);
  ::testing::Mock::VerifyAndClear(&observer2);

  PageLiveStateDecorator::RemoveAllPageObserver(&observer1);
  EXPECT_FALSE(PageLiveStateDecorator::HasAllPageObserver(&observer1));

  // Removing an observer that's not in the list is a no-op.
  PageLiveStateDecorator::RemoveAllPageObserver(&observer1);
  EXPECT_FALSE(PageLiveStateDecorator::HasAllPageObserver(&observer1));

  // `observer2` should be notified about change to `page2`.
  EXPECT_CALL(observer2, OnIsConnectedToUSBDeviceChanged(page2.get()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page2.get())
      ->SetIsConnectedToUSBDeviceForTesting(false);
  ::testing::Mock::VerifyAndClear(&observer1);

  // Removing the last node should not crash, even though there's still an
  // observer.
  contents2.reset();
  EXPECT_FALSE(page2);

  // RemoveAllPageObserver and HasAllPageObserver should not crash if called
  // after PerformanceManager teardown.
  TearDownNow();
  EXPECT_FALSE(PageLiveStateDecorator::HasAllPageObserver(&observer2));
  PageLiveStateDecorator::RemoveAllPageObserver(&observer2);
}

}  // namespace performance_manager
