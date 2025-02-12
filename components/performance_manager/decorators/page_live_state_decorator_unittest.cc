// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_capability_type.h"
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
    observer_.reset();
    DeleteContents();
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

 private:
  std::optional<TestPageLiveStateObserver> observer_;
};

TEST_F(PageLiveStateDecoratorTest, Usb) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kUSB, value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToUSBDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Bluetooth) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kBluetoothConnected,
        value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Hid) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kHID, value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToHidDevice, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToHidDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, Serial) {
  auto setter = [](content::WebContents* contents, bool value) {
    PageLiveStateDecorator::OnCapabilityTypesChanged(
        contents, content::WebContentsCapabilityType::kSerial, value);
  };
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToSerialPort, setter);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsConnectedToSerialPortChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingVideoChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingVideo,
      &PageLiveStateDecorator::OnIsCapturingVideoChanged);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingVideoChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingAudioChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingAudio,
      &PageLiveStateDecorator::OnIsCapturingAudioChanged);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingAudioChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsBeingMirroredChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsBeingMirrored,
      &PageLiveStateDecorator::OnIsBeingMirroredChanged);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsBeingMirroredChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingWindowChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingWindow,
      &PageLiveStateDecorator::OnIsCapturingWindowChanged);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsCapturingWindowChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingDisplayChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsCapturingDisplay,
      &PageLiveStateDecorator::OnIsCapturingDisplayChanged);
  VerifyObserverExpectation(TestPageLiveStateObserver::ObserverFunction::
                                kOnIsCapturingDisplayChanged);
}

TEST_F(PageLiveStateDecoratorTest, SetIsAutoDiscardable) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsAutoDiscardable,
      &PageLiveStateDecorator::SetIsAutoDiscardable,
      /*default_state=*/true);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsAutoDiscardableChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsActiveTabChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab,
      &PageLiveStateDecorator::SetIsActiveTab,
      /*default_state=*/false);
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsActiveTabChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsPinnedTabChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab,
      &PageLiveStateDecorator::SetIsPinnedTab,
      /*default_state=*/false);
  VerifyObserverExpectation(
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
  VerifyObserverExpectation(
      TestPageLiveStateObserver::ObserverFunction::kOnIsDevToolsOpenChanged);
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PageLiveStateDecoratorTest, UpdateTitleInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);
  auto* node_impl = PageNodeImpl::FromNode(node.get());
  auto* data = PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

  // Updating the title while the node is visible does nothing.
  node_impl->SetIsVisible(true);
  node_impl->OnTitleUpdated();
  EXPECT_FALSE(data->UpdatedTitleOrFaviconInBackground());

  node_impl->SetIsVisible(false);
  node_impl->OnTitleUpdated();
  EXPECT_TRUE(data->UpdatedTitleOrFaviconInBackground());
}

TEST_F(PageLiveStateDecoratorTest, UpdateFaviconInBackground) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  ASSERT_TRUE(node);
  auto* node_impl = PageNodeImpl::FromNode(node.get());
  auto* data = PageLiveStateDecorator::Data::GetOrCreateForPageNode(node.get());

  // Updating the favicon while the node is visible does nothing.
  node_impl->SetIsVisible(true);
  node_impl->OnFaviconUpdated();
  EXPECT_FALSE(data->UpdatedTitleOrFaviconInBackground());

  node_impl->SetIsVisible(false);
  node_impl->OnFaviconUpdated();
  EXPECT_TRUE(data->UpdatedTitleOrFaviconInBackground());
}

}  // namespace performance_manager
