// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"

#include <memory>
#include <utility>

#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

namespace guest_view {

namespace {

class StubGuestView : public GuestView<StubGuestView> {
 public:
  explicit StubGuestView(WebContents* owner_web_contents,
                         std::unique_ptr<WebContents> guest_web_contents)
      : GuestView<StubGuestView>(owner_web_contents) {
    WebContentsObserver::Observe(guest_web_contents.get());
    TakeGuestContentsOwnership(std::move(guest_web_contents));
  }

  explicit StubGuestView(WebContents* owner_web_contents)
      : GuestView<StubGuestView>(owner_web_contents) {}

  StubGuestView(const StubGuestView&) = delete;
  StubGuestView& operator=(const StubGuestView&) = delete;

  ~StubGuestView() override = default;

  static const char Type[];

  void AssignNewGuestContents(std::unique_ptr<WebContents> guest_web_contents) {
    ClearOwnedGuestContents();
    WebContentsObserver::Observe(guest_web_contents.get());
    TakeGuestContentsOwnership(std::move(guest_web_contents));
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
  void MaybeRecreateGuestContents(WebContents* embedder_web_contents) override {
    ADD_FAILURE();
  }
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) override {
    ADD_FAILURE();
  }
};

const char StubGuestView::Type[] = "stubguestview";

class GuestViewManagerTest : public content::RenderViewHostTestHarness {
 public:
  GuestViewManagerTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  GuestViewManagerTest(const GuestViewManagerTest&) = delete;
  GuestViewManagerTest& operator=(const GuestViewManagerTest&) = delete;

  ~GuestViewManagerTest() override = default;

  TestGuestViewManager* CreateManager() {
    return static_cast<TestGuestViewManager*>(
        GuestViewManager::CreateWithDelegate(
            browser_context(), std::make_unique<GuestViewManagerDelegate>()));
  }

  std::unique_ptr<WebContents> CreateWebContents() {
    return WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
  }

  std::unique_ptr<StubGuestView> CreateGuest(WebContents* owner_web_contents) {
    return std::make_unique<StubGuestView>(owner_web_contents,
                                           CreateWebContents());
  }

 private:
  TestGuestViewManagerFactory factory_;
};

}  // namespace

TEST_F(GuestViewManagerTest, AddRemove) {
  TestGuestViewManager* manager = CreateManager();

  std::unique_ptr<WebContents> owned_owner_web_contents(CreateWebContents());
  WebContents* owner_web_contents = owned_owner_web_contents.get();

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  std::unique_ptr<StubGuestView> guest1(CreateGuest(owner_web_contents));
  // The test assumes assigned IDs start at 1.
  ASSERT_EQ(guest1->guest_instance_id(), 1);
  std::unique_ptr<StubGuestView> guest2(CreateGuest(owner_web_contents));
  std::unique_ptr<StubGuestView> guest3(CreateGuest(owner_web_contents));

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
      std::make_unique<StubGuestView>(owner_web_contents));
  std::unique_ptr<StubGuestView> guest5(CreateGuest(owner_web_contents));
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
TEST_F(GuestViewManagerTest, ReuseIdForRecreatedGuestPage) {
  TestGuestViewManager* manager = CreateManager();

  std::unique_ptr<WebContents> owned_owner_web_contents(CreateWebContents());
  WebContents* owner_web_contents = owned_owner_web_contents.get();

  EXPECT_EQ(0, manager->last_instance_id_removed());
  ASSERT_TRUE(manager->CanUseGuestInstanceID(1));

  std::unique_ptr<StubGuestView> guest1(CreateGuest(owner_web_contents));
  manager->AddGuest(guest1.get());
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());

  manager->RemoveGuest(guest1.get(), /*invalidate_id=*/false);
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());
  EXPECT_EQ(0, manager->last_instance_id_removed());
  ASSERT_TRUE(manager->CanUseGuestInstanceID(1));

  guest1->AssignNewGuestContents(CreateWebContents());

  manager->AddGuest(guest1.get());
  EXPECT_EQ(1U, manager->GetCurrentGuestCount());

  manager->RemoveGuest(guest1.get(), /*invalidate_id=*/true);
  EXPECT_EQ(0U, manager->GetCurrentGuestCount());
  EXPECT_EQ(1, manager->last_instance_id_removed());
  ASSERT_FALSE(manager->CanUseGuestInstanceID(1));
}

}  // namespace guest_view
