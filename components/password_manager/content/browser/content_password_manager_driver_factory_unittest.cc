// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory_test_api.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content_password_manager_driver_factory_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace password_manager {

// Fixture for testing that Password Manager is enabled in fenced frames.
class ContentPasswordManagerDriverFactoryFencedFramesTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentPasswordManagerDriverFactoryFencedFramesTest() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    enabled.push_back(
        {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}});
    enabled.push_back({blink::features::kFencedFramesAPIChanges, {}});

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  ~ContentPasswordManagerDriverFactoryFencedFramesTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    factory_ = ContentPasswordManagerDriverFactoryTestApi::Create(
        web_contents(), &password_manager_client_);
  }

  void NavigateAndCommitInFrame(const std::string& url,
                                content::RenderFrameHost* rfh) {
    auto navigation =
        content::NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
    // These tests simulate loading events manually.
    navigation->SetKeepLoading(true);
    navigation->Start();
    navigation->Commit();
  }

  ContentPasswordManagerDriverFactory& factory() { return *factory_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  StubPasswordManagerClient password_manager_client_;
  std::unique_ptr<ContentPasswordManagerDriverFactory> factory_;
};

TEST_F(ContentPasswordManagerDriverFactoryFencedFramesTest,
       DisablePasswordManagerWithinFencedFrame) {
  NavigateAndCommitInFrame("http://test.org", main_rfh());
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::RenderFrameHost* fenced_frame_subframe =
      content::RenderFrameHostTester::For(fenced_frame_root)
          ->AppendChild("iframe");
  EXPECT_NE(nullptr,
            ContentPasswordManagerDriverFactoryTestApi::GetDriverForFrame(
                &factory(), main_rfh()));
  EXPECT_NE(nullptr,
            ContentPasswordManagerDriverFactoryTestApi::GetDriverForFrame(
                &factory(), fenced_frame_root));
  EXPECT_NE(nullptr,
            ContentPasswordManagerDriverFactoryTestApi::GetDriverForFrame(
                &factory(), fenced_frame_subframe));
}

class ContentPasswordManagerDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    factory_ = ContentPasswordManagerDriverFactoryTestApi::Create(
        web_contents(), &password_manager_client_);
  }

  void TearDown() override {
    factory_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Navigates `rfh` to `url` and returns the RenderFrameHost that committed.
  content::RenderFrameHost* NavigateFrame(const GURL& url,
                                          content::RenderFrameHost* rfh) {
    auto navigation =
        content::NavigationSimulator::CreateRendererInitiated(url, rfh);
    navigation->Commit();
    return navigation->GetFinalRenderFrameHost();
  }

  ContentPasswordManagerDriver* GetDriver(content::RenderFrameHost* rfh) {
    return ContentPasswordManagerDriverFactoryTestApi::GetDriverForFrame(
        factory_.get(), rfh);
  }

 private:
  StubPasswordManagerClient password_manager_client_;
  std::unique_ptr<ContentPasswordManagerDriverFactory> factory_;
};

// Tests that when a cross-document navigation in a subframe reuses the
// RenderFrameHost (and hence the driver) of the previous document, the driver
// discards its identity and state, so that state created for the previous
// document isn't associated with the new document. See crbug.com/561039723.
TEST_F(ContentPasswordManagerDriverFactoryTest,
       SubframeNavigationReusingRenderFrameHostResetsDriver) {
  NavigateFrame(GURL("https://a.test/"), main_rfh());
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  ContentPasswordManagerDriver* driver = GetDriver(child);
  ASSERT_TRUE(driver);
  const DriverId old_id = driver->GetId();
  base::WeakPtr<PasswordManagerDriver> old_weak_ptr = driver->AsWeakPtr();
  driver->GetPasswordGenerationHelper()->AddManualGenerationEnabledField(
      autofill::FieldRendererId(1));

  // The first navigation away from the initial empty document reuses the
  // RenderFrameHost.
  ASSERT_EQ(child, NavigateFrame(GURL("https://b.a.test/"), child));
  ASSERT_EQ(driver, GetDriver(child));

  EXPECT_FALSE(old_weak_ptr);
  EXPECT_NE(old_id, driver->GetId());
  EXPECT_TRUE(driver->AsWeakPtr());
  EXPECT_TRUE(driver->GetPasswordGenerationHelper()
                  ->GenerationEnabledFieldsForTests()
                  .empty());
}

// Tests that when a cross-document navigation in the main frame reuses the
// RenderFrameHost (and hence the driver) of the previous document, the driver
// is reset as well.
TEST_F(ContentPasswordManagerDriverFactoryTest,
       MainFrameNavigationReusingRenderFrameHostResetsDriver) {
  content::RenderFrameHost* rfh = main_rfh();
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  ContentPasswordManagerDriver* driver = GetDriver(rfh);
  ASSERT_TRUE(driver);
  const DriverId old_id = driver->GetId();
  base::WeakPtr<PasswordManagerDriver> old_weak_ptr = driver->AsWeakPtr();

  // The first navigation away from the initial empty document reuses the
  // RenderFrameHost.
  ASSERT_EQ(rfh, NavigateFrame(GURL("https://a.test/"), rfh));
  ASSERT_EQ(driver, GetDriver(rfh));

  EXPECT_FALSE(old_weak_ptr);
  EXPECT_NE(old_id, driver->GetId());
  EXPECT_TRUE(driver->AsWeakPtr());
}

// Tests that same-document navigations in a subframe don't reset the driver.
TEST_F(ContentPasswordManagerDriverFactoryTest,
       SameDocumentSubframeNavigationDoesNotResetDriver) {
  NavigateFrame(GURL("https://a.test/"), main_rfh());
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child = NavigateFrame(GURL("https://a.test/child"), child);
  ContentPasswordManagerDriver* driver = GetDriver(child);
  ASSERT_TRUE(driver);
  const DriverId old_id = driver->GetId();
  base::WeakPtr<PasswordManagerDriver> old_weak_ptr = driver->AsWeakPtr();

  auto same_document_navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://a.test/child#ref"), child);
  same_document_navigation->CommitSameDocument();
  ASSERT_EQ(child, same_document_navigation->GetFinalRenderFrameHost());

  EXPECT_TRUE(old_weak_ptr);
  EXPECT_EQ(old_id, driver->GetId());
}

// Fixture for testing page activations, i.e. BFCache restores and prerendered
// page activations.
class ContentPasswordManagerDriverFactoryPageActivationTest
    : public ContentPasswordManagerDriverFactoryTest {
 public:
  ContentPasswordManagerDriverFactoryPageActivationTest() {
    bfcache_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  void SetUp() override {
    ContentPasswordManagerDriverFactoryTest::SetUp();
    web_contents_delegate_.emplace(*web_contents());
  }

  void TearDown() override {
    web_contents_delegate_.reset();
    ContentPasswordManagerDriverFactoryTest::TearDown();
  }

 private:
  // Declared before `bfcache_feature_list_`, which is initialized after it, so
  // that the feature lists are reset in the reverse order of initialization.
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
  base::test::ScopedFeatureList bfcache_feature_list_;
  std::optional<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

// Tests that a BFCache restore, which reactivates an existing document rather
// than creating a new one, doesn't reset the driver of the restored frame.
TEST_F(ContentPasswordManagerDriverFactoryPageActivationTest,
       BackForwardCacheRestoreDoesNotResetDriver) {
  content::RenderFrameHostWrapper rfh(
      NavigateFrame(GURL("https://a.test/"), main_rfh()));
  ContentPasswordManagerDriver* driver = GetDriver(rfh.get());
  ASSERT_TRUE(driver);
  const DriverId old_id = driver->GetId();
  base::WeakPtr<PasswordManagerDriver> old_weak_ptr = driver->AsWeakPtr();

  NavigateFrame(GURL("https://b.test/"), main_rfh());
  ASSERT_TRUE(rfh);
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  ASSERT_EQ(rfh.get(), content::NavigationSimulator::GoBack(web_contents()));
  ASSERT_EQ(driver, GetDriver(rfh.get()));

  EXPECT_TRUE(old_weak_ptr);
  EXPECT_EQ(old_id, driver->GetId());
}

// Tests that a prerendered page activation, which reactivates an existing
// document rather than creating a new one, doesn't reset the driver of the
// activated frame.
TEST_F(ContentPasswordManagerDriverFactoryPageActivationTest,
       PrerenderedPageActivationDoesNotResetDriver) {
  NavigateFrame(GURL("https://a.test/"), main_rfh());
  const GURL prerender_url("https://a.test/prerender");
  content::RenderFrameHost* prerender_rfh =
      content::WebContentsTester::For(web_contents())
          ->AddPrerenderAndCommitNavigation(prerender_url);
  ContentPasswordManagerDriver* driver = GetDriver(prerender_rfh);
  ASSERT_TRUE(driver);
  const DriverId old_id = driver->GetId();
  base::WeakPtr<PasswordManagerDriver> old_weak_ptr = driver->AsWeakPtr();

  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(prerender_url);
  ASSERT_EQ(prerender_rfh, main_rfh());
  ASSERT_EQ(driver, GetDriver(prerender_rfh));

  EXPECT_TRUE(old_weak_ptr);
  EXPECT_EQ(old_id, driver->GetId());
}

}  // namespace password_manager
