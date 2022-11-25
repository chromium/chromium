// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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
    kOnIsActiveTabChanged,
    kOnContentSettingsChanged,
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
  void OnIsActiveTabChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnIsActiveTabChanged;
    page_node_passed_ = page_node;
  }
  void OnContentSettingsChanged(const PageNode* page_node) override {
    latest_function_called_ = ObserverFunction::kOnContentSettingsChanged;
    page_node_passed_ = page_node;
  }

  ObserverFunction latest_function_called_ = ObserverFunction::kNone;
  raw_ptr<const PageNode> page_node_passed_ = nullptr;
};

class MockPageLiveStateDelegate
    : public performance_manager::PageLiveStateDecorator::Delegate {
 public:
  MockPageLiveStateDelegate() = default;

 private:
  std::map<ContentSettingsType, ContentSetting> GetContentSettingsForUrl(
      content::WebContents* web_contents,
      const GURL& url) override {
    return {
        {ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW},
    };
  }
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
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>(
        base::SequenceBound<MockPageLiveStateDelegate>(
            content::GetUIThreadTaskRunner({}))));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return content::GetUIThreadTaskRunner({});
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

TEST_F(PageLiveStateDecoratorTest, OnIsActiveTabChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab,
      &PageLiveStateDecorator::SetIsActiveTab,
      /*default_state=*/false);
  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnIsActiveTabChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnContentSettingsChanged) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());

  {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(node);
          const PageLiveStateDecorator::Data* data =
              PageLiveStateDecorator::Data::FromPageNode(node.get());
          ASSERT_TRUE(data);
          EXPECT_EQ(data->IsContentSettingTypeAllowed(
                        ContentSettingsType::NOTIFICATIONS),
                    false);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  PageLiveStateDecorator::SetContentSettings(
      web_contents(),
      {
          {ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW},
      });

  {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(node);
          const PageLiveStateDecorator::Data* data =
              PageLiveStateDecorator::Data::FromPageNode(node.get());
          ASSERT_TRUE(data);
          EXPECT_EQ(data->IsContentSettingTypeAllowed(
                        ContentSettingsType::NOTIFICATIONS),
                    true);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnContentSettingsChanged);

  PageLiveStateDecorator::SetContentSettings(
      web_contents(),
      {
          {ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK},
      });

  {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(node);
          const PageLiveStateDecorator::Data* data =
              PageLiveStateDecorator::Data::FromPageNode(node.get());
          ASSERT_TRUE(data);
          EXPECT_EQ(data->IsContentSettingTypeAllowed(
                        ContentSettingsType::NOTIFICATIONS),
                    false);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnContentSettingsChanged);
}

// Content settings aren't fetched on navigation on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageLiveStateDecoratorTest, GetContentSettingsOnNavigation) {
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(node);
          const PageLiveStateDecorator::Data* data =
              PageLiveStateDecorator::Data::FromPageNode(node.get());
          ASSERT_TRUE(data);
          EXPECT_EQ(data->IsContentSettingTypeAllowed(
                        ContentSettingsType::NOTIFICATIONS),
                    false);
          PageNodeImpl::FromNode(node.get())
              ->OnMainFrameNavigationCommitted(
                  /*same_document=*/false,
                  /*navigation_committed_time=*/base::TimeTicks::Now(),
                  /*navigation_id=*/1,
                  /*url=*/GURL("http://www.example.com"),
                  /*contents_mime_type=*/"text/html");

          // Posting the quit_closure run on the same task runner as the content
          // settings fetch ensures it's run after the settings are done being
          // retrieved.
          task_runner()->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    std::move(quit_closure).Run();
                                  }));
        }));
    run_loop.Run();
  }

  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnContentSettingsChanged);

  {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(node);
          const PageLiveStateDecorator::Data* data =
              PageLiveStateDecorator::Data::FromPageNode(node.get());
          ASSERT_TRUE(data);
          EXPECT_EQ(data->IsContentSettingTypeAllowed(
                        ContentSettingsType::NOTIFICATIONS),
                    true);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  VerifyObserverExpectationOnPMSequence(
      TestPageLiveStateObserver::ObserverFunction::kOnContentSettingsChanged);
}
#endif

}  // namespace performance_manager
