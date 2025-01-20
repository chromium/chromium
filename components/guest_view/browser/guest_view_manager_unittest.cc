// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/guest_page_holder.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

namespace guest_view {

namespace {

class StubGuestView : public GuestView<StubGuestView> {
 public:
  explicit StubGuestView(content::RenderFrameHost* owner_rfh,
                         std::unique_ptr<WebContents> guest_web_contents)
      : GuestView<StubGuestView>(owner_rfh) {
    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
    WebContentsObserver::Observe(guest_web_contents.get());
    TakeGuestContentsOwnership(std::move(guest_web_contents));
  }

  explicit StubGuestView(content::RenderFrameHost* owner_rfh)
      : GuestView<StubGuestView>(owner_rfh) {}

  StubGuestView(const StubGuestView&) = delete;
  StubGuestView& operator=(const StubGuestView&) = delete;

  ~StubGuestView() override = default;

  static const char Type[];
  static const GuestViewHistogramValue HistogramValue =
      GuestViewHistogramValue::kInvalid;

  void AssignNewGuestContents(std::unique_ptr<WebContents> guest_web_contents) {
    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
    ClearOwnedGuestContents();
    WebContentsObserver::Observe(guest_web_contents.get());
    TakeGuestContentsOwnership(std::move(guest_web_contents));
  }

  void AssignNewGuestPage() {
    EXPECT_TRUE(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
    ClearOwnedGuestPage();

    std::unique_ptr<content::GuestPageHolder> guest_page_holder =
        content::GuestPageHolder::Create(
            owner_web_contents(),
            content::SiteInstance::Create(browser_context()),
            GetGuestPageHolderDelegateWeakPtr());

    SetGuestPageHolder(guest_page_holder.get());
    TakeGuestPageOwnership(std::move(guest_page_holder));
  }

  // Stub implementations of GuestViewBase's pure virtual methods:
  const char* GetAPINamespace() const override {
    ADD_FAILURE();
    return "";
  }
  int GetTaskPrefix() const override {
    ADD_FAILURE();
    return 0;
  }
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) override {
    ADD_FAILURE();
  }
  void CreateInnerPage(std::unique_ptr<GuestViewBase> owned_this,
                       scoped_refptr<content::SiteInstance> site_instance,
                       const base::Value::Dict& create_params,
                       GuestPageCreatedCallback callback) override {
    ADD_FAILURE();
  }

  bool GuestHandleContextMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) override {
    return false;
  }
};

const char StubGuestView::Type[] = "stubguestview";

class GuestViewManagerTest : public content::RenderViewHostTestHarness,
                             public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "MPArch" : "InnerWebContents";
  }

  GuestViewManagerTest() {
    scoped_feature_list_.InitWithFeatureState(features::kGuestViewMPArch,
                                              GetParam());
  }

  GuestViewManagerTest(const GuestViewManagerTest&) = delete;
  GuestViewManagerTest& operator=(const GuestViewManagerTest&) = delete;

  ~GuestViewManagerTest() override = default;

  TestGuestViewManager* CreateManager() {
    return factory_.GetOrCreateTestGuestViewManager(
        browser_context(), std::make_unique<GuestViewManagerDelegate>());
  }

  std::unique_ptr<WebContents> CreateWebContents() {
    return WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
  }

  std::unique_ptr<StubGuestView> CreateGuest(
      content::RenderFrameHost* owner_rfh) {
    if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
      std::unique_ptr<StubGuestView> guest =
          std::make_unique<StubGuestView>(owner_rfh);
      guest->AssignNewGuestPage();
      return guest;
    } else {
      return std::make_unique<StubGuestView>(owner_rfh, CreateWebContents());
    }
  }

 private:
  TestGuestViewManagerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         GuestViewManagerTest,
                         testing::Bool(),
                         GuestViewManagerTest::DescribeParams);

TEST_P(GuestViewManagerTest, AddRemove) {
  TestGuestViewManager* manager = CreateManager();

  std::unique_ptr<WebContents> owned_owner_web_contents(CreateWebContents());
  content::RenderFrameHost* owner_rfh =
      owned_owner_web_contents->GetPrimaryMainFrame();

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  std::unique_ptr<StubGuestView> guest1(CreateGuest(owner_rfh));
  // The test assumes assigned IDs start at 1.
  ASSERT_EQ(guest1->guest_instance_id(), 1);
  std::unique_ptr<StubGuestView> guest2(CreateGuest(owner_rfh));
  std::unique_ptr<StubGuestView> guest3(CreateGuest(owner_rfh));

  manager->AddGuest(guest1.get());
  manager->AddGuest(guest2.get());
  manager->RemoveGuest(guest2.get(), /*invalidate_id=*/true);

  // Since we removed 2, it would be an invalid ID.
  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  manager->AddGuest(guest3.get());
  manager->RemoveGuest(guest1.get(), /*invalidate_id=*/true);
  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));

  EXPECT_EQ(2, manager->last_instance_id_removed());
  manager->RemoveGuest(guest3.get(), /*invalidate_id=*/true);
  EXPECT_EQ(3, manager->last_instance_id_removed());

  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0u, manager->GetNumRemovedInstanceIDs());

  std::unique_ptr<StubGuestView> guest4(
      std::make_unique<StubGuestView>(owner_rfh));
  std::unique_ptr<StubGuestView> guest5(CreateGuest(owner_rfh));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(4));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(5));
  // Suppose a GuestView (id=4) is created, but never initialized with a guest
  // WebContents. We should be able to invalidate the id it used.
  manager->AddGuest(guest5.get());
  manager->RemoveGuest(guest5.get(), /*invalidate_id=*/true);
  EXPECT_EQ(3, manager->last_instance_id_removed());
  EXPECT_EQ(1u, manager->GetNumRemovedInstanceIDs());
  manager->RemoveGuest(guest4.get(), /*invalidate_id=*/true);
  EXPECT_FALSE(manager->CanUseGuestInstanceID(4));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(5));
  EXPECT_EQ(5, manager->last_instance_id_removed());
  EXPECT_EQ(0u, manager->GetNumRemovedInstanceIDs());
}

// This covers the case where a guest needs to recreate its guest WebContents
// before attachment. In this case, the same guest instance ID will be
// associated with different WebContents over time.
TEST_P(GuestViewManagerTest, ReuseIdForRecreatedGuestPage) {
  TestGuestViewManager* manager = CreateManager();

  std::unique_ptr<WebContents> owned_owner_web_contents(CreateWebContents());
  content::RenderFrameHost* owner_rfh =
      owned_owner_web_contents->GetPrimaryMainFrame();

  EXPECT_EQ(0, manager->last_instance_id_removed());
  ASSERT_TRUE(manager->CanUseGuestInstanceID(1));

  std::unique_ptr<StubGuestView> guest1(CreateGuest(owner_rfh));
  manager->AddGuest(guest1.get());
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());

  manager->RemoveGuest(guest1.get(), /*invalidate_id=*/false);
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());
  EXPECT_EQ(0, manager->last_instance_id_removed());
  ASSERT_TRUE(manager->CanUseGuestInstanceID(1));

  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    guest1->AssignNewGuestPage();
  } else {
    guest1->AssignNewGuestContents(CreateWebContents());
  }

  manager->AddGuest(guest1.get());
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());

  manager->RemoveGuest(guest1.get(), /*invalidate_id=*/true);
  EXPECT_EQ(0U, manager->GetCurrentGuestCount());
  EXPECT_EQ(1, manager->last_instance_id_removed());
  ASSERT_FALSE(manager->CanUseGuestInstanceID(1));
}

}  // namespace guest_view
