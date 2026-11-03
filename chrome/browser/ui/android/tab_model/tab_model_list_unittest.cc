// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

class TabModelListTest : public ChromeRenderViewHostTestHarness {};

class PopupCapturingTabModel : public OwningTestTabModel {
 public:
  using OwningTestTabModel::OwningTestTabModel;
  using TabModel::SetSessionId;

  // OwningTestTabModel:
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {
    ++popup_count_;
    last_parent_id_ =
        parent ? parent->GetAndroidId() : TabAndroid::kInvalidTabId;
  }

  int popup_count() const { return popup_count_; }
  int last_parent_id() const { return last_parent_id_; }

 private:
  int popup_count_ = 0;
  int last_parent_id_ = TabAndroid::kInvalidTabId;
};

}  // namespace

// Regression test for http://crbug.com/40393627.
TEST_F(TabModelListTest, TestGetTabModelForWebContents) {
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> contents(CreateTestWebContents());

  // Should not crash when there are no tabs.
  EXPECT_EQ(NULL, TabModelList::GetTabModelForWebContents(contents.get()));

  // Should not crash when there is an uninitialized tab, i.e. when
  // TabModel::GetTabAt returns NULL.
  tab_model.SetTabCount(1);
  EXPECT_EQ(NULL, TabModelList::GetTabModelForWebContents(contents.get()));

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(TabModelListTest,
       HandlePopupNavigationUsesContainingTabModelWhenUnassociated) {
  PopupCapturingTabModel first_model(profile());
  PopupCapturingTabModel second_model(profile());
  ASSERT_FALSE(first_model.GetSessionId().is_valid());
  ASSERT_FALSE(second_model.GetSessionId().is_valid());

  TabAndroid* opener = second_model.AddTabFromWebContents(
      CreateTestWebContents(), /*index=*/0, /*select=*/true);
  ASSERT_NE(nullptr, opener);
  ASSERT_FALSE(opener->GetWindowId().is_valid());

  NavigateParams params(profile(), GURL("https://example.com"),
                        ui::PAGE_TRANSITION_LINK);
  params.source_contents = opener->web_contents();
  TabModelList::HandlePopupNavigation(&params);

  EXPECT_EQ(0, first_model.popup_count());
  EXPECT_EQ(1, second_model.popup_count());
  EXPECT_EQ(opener->GetAndroidId(), second_model.last_parent_id());
}

TEST_F(TabModelListTest,
       HandlePopupNavigationIgnoresDetachedTabWithInvalidSessionId) {
  PopupCapturingTabModel model(profile());
  ASSERT_FALSE(model.GetSessionId().is_valid());

  std::unique_ptr<TabAndroid> detached_tab = TabAndroid::CreateForTesting(
      profile(), /*tab_id=*/42, CreateTestWebContents());
  ASSERT_FALSE(detached_tab->GetWindowId().is_valid());

  NavigateParams params(profile(), GURL("https://example.com"),
                        ui::PAGE_TRANSITION_LINK);
  params.source_contents = detached_tab->web_contents();
  TabModelList::HandlePopupNavigation(&params);

  EXPECT_EQ(0, model.popup_count());
}

TEST_F(TabModelListTest, HandlePopupNavigationFallsBackToValidWindowSessionId) {
  PopupCapturingTabModel unassociated_model(profile());
  PopupCapturingTabModel associated_model(profile());
  const SessionID window_id = SessionID::NewUnique();
  associated_model.SetSessionId(window_id);
  ASSERT_FALSE(unassociated_model.GetSessionId().is_valid());
  ASSERT_EQ(window_id, associated_model.GetSessionId());

  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  sessions::SessionTabHelper::CreateForWebContents(contents.get(),
                                                   base::NullCallback());
  std::unique_ptr<TabAndroid> detached_tab = TabAndroid::CreateForTesting(
      profile(), /*tab_id=*/99, std::move(contents));
  detached_tab->SetWindowSessionID(window_id);

  NavigateParams params(profile(), GURL("https://example.com"),
                        ui::PAGE_TRANSITION_LINK);
  params.source_contents = detached_tab->web_contents();
  TabModelList::HandlePopupNavigation(&params);

  EXPECT_EQ(0, unassociated_model.popup_count());
  EXPECT_EQ(1, associated_model.popup_count());
  EXPECT_EQ(99, associated_model.last_parent_id());
}

TEST_F(TabModelListTest, FindLiveTabContextWithIDIsScopedToProfile) {
  std::unique_ptr<TestingProfile> other_profile =
      TestingProfile::Builder().Build();

  PopupCapturingTabModel other_model(other_profile.get());
  const SessionID other_window_id = SessionID::NewUnique();
  other_model.SetSessionId(other_window_id);

  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PopupCapturingTabModel otr_model(otr_profile);
  const SessionID otr_window_id = SessionID::NewUnique();
  otr_model.SetSessionId(otr_window_id);

  ChromeTabRestoreServiceClient chrome_client(profile());
  sessions::TabRestoreServiceClient& client = chrome_client;
  const SessionID unknown_window_id = SessionID::NewUnique();

  // When `profile()` has no open non-OTR TabModel, lookup and fallback must not
  // return `other_profile`'s TabModel or `profile()`'s OTR TabModel.
  EXPECT_EQ(nullptr, AndroidLiveTabContext::FindContextWithID(unknown_window_id,
                                                              profile()));
  EXPECT_EQ(nullptr, AndroidLiveTabContext::FindContextWithID(other_window_id,
                                                              profile()));
  EXPECT_EQ(nullptr,
            AndroidLiveTabContext::FindContextWithID(otr_window_id, profile()));
  EXPECT_EQ(nullptr, client.FindLiveTabContextWithID(unknown_window_id));
  EXPECT_EQ(nullptr, client.FindLiveTabContextWithID(other_window_id));
  EXPECT_EQ(nullptr, client.FindLiveTabContextWithID(otr_window_id));

  // Register two non-OTR TabModels for `profile()` after `other_model` and
  // `otr_model`.
  PopupCapturingTabModel profile_model(profile());
  const SessionID profile_window_id = SessionID::NewUnique();
  profile_model.SetSessionId(profile_window_id);

  PopupCapturingTabModel second_profile_model(profile());
  const SessionID second_profile_window_id = SessionID::NewUnique();
  second_profile_model.SetSessionId(second_profile_window_id);

  EXPECT_EQ(
      profile_model.GetLiveTabContext(),
      AndroidLiveTabContext::FindContextWithID(profile_window_id, profile()));
  EXPECT_EQ(second_profile_model.GetLiveTabContext(),
            AndroidLiveTabContext::FindContextWithID(second_profile_window_id,
                                                     profile()));
  EXPECT_EQ(
      profile_model.GetLiveTabContext(),
      AndroidLiveTabContext::FindContextWithID(unknown_window_id, profile()));
  EXPECT_EQ(
      profile_model.GetLiveTabContext(),
      AndroidLiveTabContext::FindContextWithID(other_window_id, profile()));
  EXPECT_EQ(profile_model.GetLiveTabContext(),
            AndroidLiveTabContext::FindContextWithID(otr_window_id, profile()));

  EXPECT_EQ(profile_model.GetLiveTabContext(),
            client.FindLiveTabContextWithID(profile_window_id));
  EXPECT_EQ(second_profile_model.GetLiveTabContext(),
            client.FindLiveTabContextWithID(second_profile_window_id));
  EXPECT_EQ(profile_model.GetLiveTabContext(),
            client.FindLiveTabContextWithID(unknown_window_id));
  EXPECT_EQ(profile_model.GetLiveTabContext(),
            client.FindLiveTabContextWithID(other_window_id));
  EXPECT_EQ(profile_model.GetLiveTabContext(),
            client.FindLiveTabContextWithID(otr_window_id));
}
