// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

std::vector<TabModel*> CollectModels(const Profile* profile) {
  std::vector<TabModel*> visited;
  TabModelList::ForEach(profile, [&](TabModel* model) {
    visited.push_back(model);
    return true;
  });
  return visited;
}

std::vector<TabModel*> CollectAllModels() {
  std::vector<TabModel*> visited;
  TabModelList::ForEachAll([&](TabModel* model) {
    visited.push_back(model);
    return true;
  });
  return visited;
}

class TabModelListTest : public ChromeRenderViewHostTestHarness {
 public:
  void TearDown() override {
    EXPECT_TRUE(CollectAllModels().empty());
    ChromeRenderViewHostTestHarness::TearDown();
  }
};

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

class NonEditableTestTabModel : public OwningTestTabModel {
 public:
  using OwningTestTabModel::OwningTestTabModel;

  // OwningTestTabModel:
  bool IsThisTabListEditable() override { return false; }
};

struct RecordingObserver : public TabModelListObserver {
  ~RecordingObserver() override { TabModelList::RemoveGlobalObserver(this); }

  void OnTabModelAdded(TabModel* tab_model) override {
    added.push_back(tab_model);
    if (on_added) {
      on_added.Run(tab_model);
    }
  }
  void OnTabModelRemoved(TabModel* tab_model) override {
    removed.push_back(tab_model);
    if (on_removed) {
      on_removed.Run(tab_model);
    }
  }

  std::vector<TabModel*> added;
  std::vector<TabModel*> removed;
  base::RepeatingCallback<void(TabModel*)> on_added;
  base::RepeatingCallback<void(TabModel*)> on_removed;
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

TEST_F(TabModelListTest, ProfileKeyedAndGlobalIteration) {
  Profile* otr = profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<TestingProfile> other = TestingProfile::Builder().Build();
  const Profile* const_profile = profile();

  OwningTestTabModel reg1(profile());
  OwningTestTabModel otr1(otr);
  OwningTestTabModel other1(other.get());
  OwningTestTabModel reg2(profile());

  EXPECT_EQ((std::vector<TabModel*>{&reg1, &reg2}),
            CollectModels(const_profile));
  EXPECT_EQ((std::vector<TabModel*>{&otr1}), CollectModels(otr));
  EXPECT_EQ((std::vector<TabModel*>{&other1}), CollectModels(other.get()));
  EXPECT_TRUE(CollectModels(nullptr).empty());
  EXPECT_EQ((std::vector<TabModel*>{&reg1, &otr1, &other1, &reg2}),
            CollectAllModels());

  EXPECT_TRUE(TabModelList::Contains(&reg1));
  EXPECT_FALSE(TabModelList::Contains(nullptr));
  TabModelList::RemoveTabModel(&reg1);
  EXPECT_FALSE(TabModelList::Contains(&reg1));
  EXPECT_EQ((std::vector<TabModel*>{&reg2}), CollectModels(const_profile));
}

TEST_F(TabModelListTest, ForEachEarlyTerminationAndReentrantMutation) {
  TestTabModel m1(profile());
  TestTabModel m2(profile());
  TestTabModel m3(profile());
  TabModelList::AddTabModel(&m1);
  TabModelList::AddTabModel(&m2);

  std::vector<TabModel*> early;
  TabModelList::ForEach(profile(), [&](TabModel* model) {
    early.push_back(model);
    return false;
  });
  EXPECT_EQ((std::vector<TabModel*>{&m1}), early);

  early.clear();
  TabModelList::ForEachAll([&](TabModel* model) {
    early.push_back(model);
    return false;
  });
  EXPECT_EQ((std::vector<TabModel*>{&m1}), early);

  // Removing `m2` and adding `m3` during `ForEach` skips `m2` and defers `m3`.
  std::vector<TabModel*> visited;
  TabModelList::ForEach(profile(), [&](TabModel* model) {
    visited.push_back(model);
    if (model == &m1) {
      TabModelList::RemoveTabModel(&m2);
      TabModelList::AddTabModel(&m3);
    }
    return true;
  });
  EXPECT_EQ((std::vector<TabModel*>{&m1}), visited);
  EXPECT_EQ((std::vector<TabModel*>{&m1, &m3}), CollectModels(profile()));

  // Same guarantee for `ForEachAll`.
  visited.clear();
  TabModelList::ForEachAll([&](TabModel* model) {
    visited.push_back(model);
    if (model == &m1) {
      TabModelList::RemoveTabModel(&m3);
      TabModelList::AddTabModel(&m2);
    }
    return true;
  });
  EXPECT_EQ((std::vector<TabModel*>{&m1}), visited);
  EXPECT_EQ((std::vector<TabModel*>{&m1, &m2}), CollectAllModels());
  TabModelList::RemoveTabModel(&m1);
  TabModelList::RemoveTabModel(&m2);
}

TEST_F(TabModelListTest, ProfileScopedAndGlobalObservers) {
  Profile* otr = profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<TestingProfile> other = TestingProfile::Builder().Build();

  RecordingObserver reg_obs, otr_obs, other_obs, global_obs;
  TabModelList::AddObserver(&reg_obs, profile());
  TabModelList::AddObserver(&otr_obs, otr);
  TabModelList::AddObserver(&other_obs, other.get());
  TabModelList::AddGlobalObserver(&global_obs);

  {
    OwningTestTabModel reg_model(profile());
    OwningTestTabModel otr_model(otr);
    OwningTestTabModel other_model(other.get());

    EXPECT_EQ((std::vector<TabModel*>{&reg_model}), reg_obs.added);
    EXPECT_EQ((std::vector<TabModel*>{&otr_model}), otr_obs.added);
    EXPECT_EQ((std::vector<TabModel*>{&other_model}), other_obs.added);
    EXPECT_EQ((std::vector<TabModel*>{&reg_model, &otr_model, &other_model}),
              global_obs.added);
  }

  EXPECT_EQ(1u, reg_obs.removed.size());
  EXPECT_EQ(1u, otr_obs.removed.size());
  EXPECT_EQ(1u, other_obs.removed.size());
  EXPECT_EQ(3u, global_obs.removed.size());

  const Profile* const_profile = profile();
  TabModelList::RemoveObserver(&reg_obs, const_profile);
  TabModelList::RemoveObserver(&otr_obs, otr);
  TabModelList::RemoveObserver(&other_obs, other.get());
  TabModelList::RemoveGlobalObserver(&global_obs);

  OwningTestTabModel extra_model(profile());
  EXPECT_EQ(1u, reg_obs.added.size());
  EXPECT_EQ(3u, global_obs.added.size());
}

TEST_F(TabModelListTest, ObserverReentrantMutation) {
  RecordingObserver self_removing_obs, sibling_obs;
  TabModelList::AddObserver(&self_removing_obs, profile());
  TabModelList::AddObserver(&sibling_obs, profile());
  self_removing_obs.on_added = base::BindLambdaForTesting([&](TabModel*) {
    TabModelList::RemoveObserver(&self_removing_obs, profile());
  });

  {
    OwningTestTabModel m1(profile());
    OwningTestTabModel m2(profile());
    EXPECT_EQ((std::vector<TabModel*>{&m1}), self_removing_obs.added);
    EXPECT_EQ((std::vector<TabModel*>{&m1, &m2}), sibling_obs.added);
  }
  TabModelList::RemoveObserver(&sibling_obs, profile());

  // Late observer registration during global notification sees the model
  // exactly once via `ForEach` on add (not twice) and receives zero spurious
  // removal notifications on remove.
  RecordingObserver late_global_obs, late_profile_obs;
  TabModelList::AddGlobalObserver(&late_global_obs);
  late_global_obs.on_added = base::BindLambdaForTesting([&](TabModel*) {
    TabModelList::AddObserver(&late_profile_obs, profile());
    TabModelList::ForEach(profile(), [&](TabModel* m) {
      late_profile_obs.added.push_back(m);
      return true;
    });
  });
  late_global_obs.on_removed = base::BindLambdaForTesting([&](TabModel*) {
    TabModelList::AddObserver(&late_profile_obs, profile());
  });

  TestTabModel late_model(profile());
  TabModelList::AddTabModel(&late_model);
  EXPECT_EQ(1u, late_profile_obs.added.size());

  TabModelList::RemoveObserver(&late_profile_obs, profile());
  TabModelList::RemoveTabModel(&late_model);
  EXPECT_EQ(0u, late_profile_obs.removed.size());

  TabModelList::RemoveObserver(&late_profile_obs, profile());
  TabModelList::RemoveGlobalObserver(&late_global_obs);

  // Global observer removing the first profile observer mid-notification skips
  // the removed observer and still notifies remaining profile observers.
  RecordingObserver removing_global_obs, first_profile_obs,
      remaining_profile_obs;
  TabModelList::AddGlobalObserver(&removing_global_obs);
  TabModelList::AddObserver(&first_profile_obs, profile());
  TabModelList::AddObserver(&remaining_profile_obs, profile());
  removing_global_obs.on_added = base::BindLambdaForTesting([&](TabModel*) {
    TabModelList::RemoveObserver(&first_profile_obs, profile());
  });
  removing_global_obs.on_removed = base::BindLambdaForTesting([&](TabModel*) {
    TabModelList::RemoveObserver(&first_profile_obs, profile());
  });

  TestTabModel removed_first_obs_model(profile());
  TabModelList::AddTabModel(&removed_first_obs_model);
  EXPECT_TRUE(first_profile_obs.added.empty());
  EXPECT_EQ((std::vector<TabModel*>{&removed_first_obs_model}),
            remaining_profile_obs.added);

  TabModelList::RemoveObserver(&remaining_profile_obs, profile());
  TabModelList::AddObserver(&first_profile_obs, profile());
  TabModelList::AddObserver(&remaining_profile_obs, profile());
  TabModelList::RemoveTabModel(&removed_first_obs_model);
  EXPECT_TRUE(first_profile_obs.removed.empty());
  EXPECT_EQ((std::vector<TabModel*>{&removed_first_obs_model}),
            remaining_profile_obs.removed);

  TabModelList::RemoveObserver(&remaining_profile_obs, profile());
  TabModelList::RemoveGlobalObserver(&removing_global_obs);
}

TEST_F(TabModelListTest, ProfileShutdownLifecycle) {
  // Destroying a Profile while its TabModel and archived TabModel remain
  // registered tears down profile-scoped state cleanly while global removal
  // still cleans up without touching Profile*.
  {
    std::unique_ptr<TestingProfile> transient_profile =
        TestingProfile::Builder().Build();
    TestTabModel model(transient_profile.get());
    TestTabModel archived(transient_profile.get(),
                          chrome::android::ActivityType::kTabbed,
                          TabModel::TabModelType::kArchived);
    RecordingObserver obs;
    TabModelList::AddObserver(&obs, transient_profile.get());
    TabModelList::AddTabModel(&model);
    TabModelList::SetArchivedTabModel(&archived);

    TabModelList::RemoveObserver(&obs, transient_profile.get());
    transient_profile.reset();

    EXPECT_EQ((std::vector<TabModel*>{&model}), CollectAllModels());

    TabModelList::RemoveArchivedTabModel(&archived);
    TabModelList::RemoveTabModel(&model);
  }

  // Profile-scoped lookups and `RemoveObserver` never dereference an unknown
  // `const Profile*` pointer.
  {
    RecordingObserver obs;
    alignas(Profile) std::byte uninitialized_profile_storage[sizeof(void*)];
    const auto* unknown_profile =
        reinterpret_cast<const Profile*>(uninitialized_profile_storage);
    EXPECT_TRUE(CollectModels(unknown_profile).empty());
    EXPECT_EQ(nullptr, TabModelList::GetArchivedTabModel(unknown_profile));
    TabModelList::RemoveObserver(&obs, unknown_profile);
  }

  // Destroying a Profile from a global observer during `AddTabModel` skips
  // profile-scoped notifications safely.
  {
    std::unique_ptr<TestingProfile> transient_profile =
        TestingProfile::Builder().Build();
    TestTabModel model(transient_profile.get());
    RecordingObserver global_obs, profile_obs;
    TabModelList::AddGlobalObserver(&global_obs);
    TabModelList::AddObserver(&profile_obs, transient_profile.get());
    global_obs.on_added = base::BindLambdaForTesting([&](TabModel*) {
      TabModelList::RemoveObserver(&profile_obs, transient_profile.get());
      transient_profile.reset();
    });

    TabModelList::AddTabModel(&model);
    EXPECT_EQ(nullptr, transient_profile);
    EXPECT_TRUE(profile_obs.added.empty());

    TabModelList::RemoveGlobalObserver(&global_obs);
    TabModelList::RemoveTabModel(&model);
  }

  // Destroying a Profile from the first profile observer during
  // `RemoveTabModel` stops notifying remaining profile observers safely.
  {
    std::unique_ptr<TestingProfile> transient_profile =
        TestingProfile::Builder().Build();
    TestTabModel model(transient_profile.get());
    TabModelList::AddTabModel(&model);
    RecordingObserver first_obs, second_obs;
    TabModelList::AddObserver(&first_obs, transient_profile.get());
    TabModelList::AddObserver(&second_obs, transient_profile.get());
    first_obs.on_removed = base::BindLambdaForTesting([&](TabModel*) {
      TabModelList::RemoveObserver(&first_obs, transient_profile.get());
      TabModelList::RemoveObserver(&second_obs, transient_profile.get());
      transient_profile.reset();
    });

    TabModelList::RemoveTabModel(&model);
    EXPECT_EQ(nullptr, transient_profile);
    EXPECT_EQ((std::vector<TabModel*>{&model}), first_obs.removed);
    EXPECT_TRUE(second_obs.removed.empty());
  }
}

TEST_F(TabModelListTest, CanEditTabListAndArchivedTabModel) {
  std::unique_ptr<TestingProfile> other = TestingProfile::Builder().Build();

  NonEditableTestTabModel other_locked(other.get());
  NonEditableTestTabModel headless_locked(
      profile(), chrome::android::ActivityType::kTabbed,
      TabModel::TabModelType::kHeadless);
  NonEditableTestTabModel cct_placeholder_locked(
      profile(), chrome::android::ActivityType::kCustomTab,
      TabModel::TabModelType::kStandard,
      chrome::android::CustomTabProfileType::kIncognito);
  EXPECT_TRUE(TabListInterface::CanEditTabList(*profile()));
  EXPECT_FALSE(TabListInterface::CanEditTabList(*other));

  {
    NonEditableTestTabModel reg_locked(profile());
    EXPECT_FALSE(TabListInterface::CanEditTabList(*profile()));
  }
  EXPECT_TRUE(TabListInterface::CanEditTabList(*profile()));

  TestTabModel archived1(profile(), chrome::android::ActivityType::kTabbed,
                         TabModel::TabModelType::kArchived);
  TestTabModel archived2(profile(), chrome::android::ActivityType::kTabbed,
                         TabModel::TabModelType::kArchived);
  TestTabModel other_archived(other.get(),
                              chrome::android::ActivityType::kTabbed,
                              TabModel::TabModelType::kArchived);
  EXPECT_EQ(nullptr, TabModelList::GetArchivedTabModel(nullptr));
  EXPECT_EQ(nullptr, TabModelList::GetArchivedTabModel(profile()));

  TabModelList::SetArchivedTabModel(&archived1);
  TabModelList::SetArchivedTabModel(&other_archived);
  EXPECT_EQ(&archived1, TabModelList::GetArchivedTabModel(profile()));
  EXPECT_EQ(&other_archived, TabModelList::GetArchivedTabModel(other.get()));

  TabModelList::SetArchivedTabModel(&archived2);
  EXPECT_EQ(&archived2, TabModelList::GetArchivedTabModel(profile()));

  TabModelList::RemoveArchivedTabModel(&archived2);
  EXPECT_EQ(nullptr, TabModelList::GetArchivedTabModel(profile()));
  EXPECT_EQ(&other_archived, TabModelList::GetArchivedTabModel(other.get()));

  TabModelList::RemoveArchivedTabModel(&other_archived);
  EXPECT_EQ(nullptr, TabModelList::GetArchivedTabModel(other.get()));
}

TEST(TabModelListPreUiThreadTest, GlobalObserverLifecycleAroundUiThreadInit) {
  ASSERT_FALSE(
      content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  RecordingObserver removed_before_init_observer;
  TabModelList::AddGlobalObserver(&removed_before_init_observer);
  TabModelList::RemoveGlobalObserver(&removed_before_init_observer);

  RecordingObserver persistent_observer;
  TabModelList::AddGlobalObserver(&persistent_observer);

  std::optional<content::BrowserTaskEnvironment> task_environment;
  task_environment.emplace();
  ASSERT_TRUE(
      content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  auto model = std::make_unique<TestTabModel>(profile.get());
  TabModel* model_ptr = model.get();
  TabModelList::AddTabModel(model_ptr);

  EXPECT_TRUE(removed_before_init_observer.added.empty());
  EXPECT_EQ((std::vector<TabModel*>{model_ptr}), persistent_observer.added);

  // Tear down `profile` and `BrowserTaskEnvironment` before destroying `model`
  // and removing `persistent_observer`, exercising post-`BrowserThread::UI`
  // teardown cleanup (`~TestTabModel()` and `RemoveGlobalObserver()`).
  profile.reset();
  task_environment.reset();
  ASSERT_FALSE(
      content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  model.reset();
  TabModelList::RemoveGlobalObserver(&persistent_observer);

  EXPECT_EQ((std::vector<TabModel*>{model_ptr}), persistent_observer.removed);
  EXPECT_TRUE(CollectAllModels().empty());
}
