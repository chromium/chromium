// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"

#include <memory>
#include <utility>

#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

namespace guest_view {

namespace {

class GuestViewManagerTest : public content::RenderViewHostTestHarness {
 public:
  GuestViewManagerTest() {}

  GuestViewManagerTest(const GuestViewManagerTest&) = delete;
  GuestViewManagerTest& operator=(const GuestViewManagerTest&) = delete;

  ~GuestViewManagerTest() override {}

  std::unique_ptr<WebContents> CreateWebContents() {
    return WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
  }
};

}  // namespace

TEST_F(GuestViewManagerTest, AddRemove) {
  std::unique_ptr<GuestViewManagerDelegate> delegate(
      new GuestViewManagerDelegate());
  std::unique_ptr<TestGuestViewManager> manager(
      new TestGuestViewManager(browser_context(), std::move(delegate)));

  std::unique_ptr<WebContents> web_contents1(CreateWebContents());
  std::unique_ptr<WebContents> web_contents2(CreateWebContents());
  std::unique_ptr<WebContents> web_contents3(CreateWebContents());

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  manager->AddGuest(1, web_contents1.get());
  manager->AddGuest(2, web_contents2.get());
  manager->RemoveGuest(2);

  // Since we removed 2, it would be an invalid ID.
  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  manager->AddGuest(3, web_contents3.get());
  manager->RemoveGuest(1);
  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));

  EXPECT_EQ(2, manager->last_instance_id_removed());
  manager->RemoveGuest(3);
  EXPECT_EQ(3, manager->last_instance_id_removed());

  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0u, manager->GetNumRemovedInstanceIDs());

  std::unique_ptr<WebContents> web_contents5(CreateWebContents());
  EXPECT_TRUE(manager->CanUseGuestInstanceID(4));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(5));
  // Suppose a GuestView (id=4) is created, but never initialized with a guest
  // WebContents. We should be able to invalidate the id it used.
  manager->AddGuest(5, web_contents5.get());
  manager->RemoveGuest(5);
  EXPECT_EQ(3, manager->last_instance_id_removed());
  EXPECT_EQ(1u, manager->GetNumRemovedInstanceIDs());
  manager->RemoveGuest(4);
  EXPECT_FALSE(manager->CanUseGuestInstanceID(4));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(5));
  EXPECT_EQ(5, manager->last_instance_id_removed());
  EXPECT_EQ(0u, manager->GetNumRemovedInstanceIDs());
}

}  // namespace guest_view
