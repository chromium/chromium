// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/interaction/interactive_views_test.h"

class ProjectsPanelTabGroupsInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ProjectsPanelTabGroupsInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(/*enabled_features=*/
                                          {tabs::kVerticalTabs,
                                           tab_groups::kProjectsPanel},
                                          /*disabled_features=*/{});
    ProjectsPanelView::disable_animations_for_testing();
  }
  ~ProjectsPanelTabGroupsInteractiveUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InteractiveBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ProjectsPanelTabGroupsInteractiveUiTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    tab_groups::TabGroupSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&ProjectsPanelTabGroupsInteractiveUiTest::
                                         CreateMockTabGroupSyncService,
                                     base::Unretained(this)));
  }

  // TODO(crbug.com/481447536): Look into having this method be reusable across
  // tests.
  std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
      content::BrowserContext* context) {
    auto mock_service = std::make_unique<
        testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
    mock_tab_group_service_ = mock_service.get();

    // Set up the service to handle observers and group accesses.
    ON_CALL(*mock_tab_group_service_, AddObserver(testing::_))
        .WillByDefault(
            [this](tab_groups::TabGroupSyncService::Observer* observer) {
              tab_group_service_observers_.AddObserver(observer);
            });
    ON_CALL(*mock_tab_group_service_, RemoveObserver(testing::_))
        .WillByDefault(
            [this](tab_groups::TabGroupSyncService::Observer* observer) {
              tab_group_service_observers_.RemoveObserver(observer);
            });
    ON_CALL(*mock_tab_group_service_, GetAllGroups()).WillByDefault([this]() {
      return mock_groups_;
    });
    ON_CALL(*mock_tab_group_service_,
            GetGroup(testing::An<const base::Uuid&>()))
        .WillByDefault([this](const base::Uuid& guid) {
          auto it = std::ranges::find(mock_groups_, guid,
                                      &tab_groups::SavedTabGroup::saved_guid);
          return it != mock_groups_.end() ? std::make_optional(*it)
                                          : std::nullopt;
        });

    return mock_service;
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

  void TearDownOnMainThread() override {
    mock_tab_group_service_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  auto OpenProjectsPanel() {
    return Steps(EnsurePresent(kVerticalTabStripProjectsButtonElementId),
                 MoveMouseTo(kVerticalTabStripProjectsButtonElementId),
                 ClickMouse(), WaitForShow(kProjectsPanelViewElementId));
  }

  auto AddTabGroup(const std::u16string& title, bool shared = false) {
    return Do([this, title, shared]() {
      static int next_uuid_suffix = 1;
      base::Uuid group_guid = base::Uuid::ParseLowercase(base::StringPrintf(
          "00000000-0000-0000-0000-%012d", next_uuid_suffix++));
      tab_groups::SavedTabGroupTab tab(GURL("about:blank"), u"About Blank",
                                       group_guid, /*position=*/0);
      tab_groups::SavedTabGroup group(title, tab_groups::TabGroupColorId::kBlue,
                                      {tab}, /*position=*/std::nullopt,
                                      group_guid);
      if (shared) {
        group.SetCollaborationId(syncer::CollaborationId("test_collaboration"));
      }
      mock_groups_.push_back(group);

      // Notify observers of the added group.
      for (auto& observer : tab_group_service_observers_) {
        observer.OnTabGroupAdded(group, tab_groups::TriggerSource::REMOTE);
      }
    });
  }

 protected:
  raw_ptr<tab_groups::MockTabGroupSyncService> mock_tab_group_service_ =
      nullptr;
  std::vector<tab_groups::SavedTabGroup> mock_groups_;
  base::ObserverList<tab_groups::TabGroupSyncService::Observer>
      tab_group_service_observers_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// TODO(crbug.com/481446933): Flaky on Windows.
// TODO(crbug.com/40759171): Flaky on Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_MoreButtonVisibilityOnHover DISABLED_MoreButtonVisibilityOnHover
#else
#define MAYBE_MoreButtonVisibilityOnHover MoreButtonVisibilityOnHover
#endif
IN_PROC_BROWSER_TEST_F(ProjectsPanelTabGroupsInteractiveUiTest,
                       MAYBE_MoreButtonVisibilityOnHover) {
  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      AddTabGroup(u"Test Group"), OpenProjectsPanel(),
      // Hover over the item and check More button visibility.
      WaitForShow(kProjectsPanelTabGroupsItemViewElementId),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return !view->more_button_for_testing()->GetVisible();
                }),
      MoveMouseTo(kProjectsPanelTabGroupsItemViewElementId),
      Do([this]() { RunScheduledLayouts(); }),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return view->more_button_for_testing()->GetVisible();
                }),
      // Move mouse away and check that More button is hidden.
      MoveMouseTo(kOmniboxElementId), Do([this]() { RunScheduledLayouts(); }),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return !view->more_button_for_testing()->GetVisible();
                }));
}

// TODO(crbug.com/481446933): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SharedIconInteractionWithMoreButton \
  DISABLED_SharedIconInteractionWithMoreButton
#else
#define MAYBE_SharedIconInteractionWithMoreButton \
  SharedIconInteractionWithMoreButton
#endif
IN_PROC_BROWSER_TEST_F(ProjectsPanelTabGroupsInteractiveUiTest,
                       MAYBE_SharedIconInteractionWithMoreButton) {
  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      AddTabGroup(u"Shared Group", /*shared=*/true), OpenProjectsPanel(),
      WaitForShow(kProjectsPanelTabGroupsItemViewElementId),
      Do([this]() { RunScheduledLayouts(); }),
      // Initially, More button is hidden, Shared icon is visible.
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return !view->more_button_for_testing()->GetVisible() &&
                         view->shared_icon_for_testing()->GetVisible();
                }),
      // Hover to show More button and hide Shared icon.
      MoveMouseTo(kProjectsPanelTabGroupsItemViewElementId),
      Do([this]() { RunScheduledLayouts(); }),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return view->more_button_for_testing()->GetVisible() &&
                         !view->shared_icon_for_testing()->GetVisible();
                }),
      // Move mouse away to revert.
      MoveMouseTo(kOmniboxElementId), Do([this]() { RunScheduledLayouts(); }),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return !view->more_button_for_testing()->GetVisible() &&
                         view->shared_icon_for_testing()->GetVisible();
                }));
}

IN_PROC_BROWSER_TEST_F(ProjectsPanelTabGroupsInteractiveUiTest,
                       ClickGroupResumesGroup) {
  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      AddTabGroup(u"Test Group"), OpenProjectsPanel(),
      WaitForShow(kProjectsPanelTabGroupsItemViewElementId), Do([this]() {
        EXPECT_CALL(*mock_tab_group_service_,
                    OpenTabGroup(mock_groups_[0].saved_guid(), testing::_))
            .WillOnce(testing::Return(std::nullopt));
      }),
      MoveMouseTo(kProjectsPanelTabGroupsItemViewElementId), ClickMouse(),
      WaitForHide(kProjectsPanelViewElementId));
}

// TODO(crbug.com/40759171): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MoreButtonClickShowsMenu DISABLED_MoreButtonClickShowsMenu
#else
#define MAYBE_MoreButtonClickShowsMenu MoreButtonClickShowsMenu
#endif
IN_PROC_BROWSER_TEST_F(ProjectsPanelTabGroupsInteractiveUiTest,
                       MAYBE_MoreButtonClickShowsMenu) {
  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      AddTabGroup(u"Test Group"), OpenProjectsPanel(),
      WaitForShow(kProjectsPanelTabGroupsItemViewElementId),
      // Hover to make More button visible.
      MoveMouseTo(kProjectsPanelTabGroupsItemViewElementId),
      Do([this]() { RunScheduledLayouts(); }),
      CheckView(kProjectsPanelTabGroupsItemViewElementId,
                [](ProjectsPanelTabGroupsItemView* view) {
                  return view->more_button_for_testing()->GetVisible();
                }),
      // Name the more button so we can click it.
      NameViewRelative(kProjectsPanelTabGroupsItemViewElementId, "MoreButton",
                       [](ProjectsPanelTabGroupsItemView* view) {
                         return view->more_button_for_testing();
                       }),
      MoveMouseTo("MoreButton"), ClickMouse(),
      Do([this]() { RunScheduledLayouts(); }),
      // Verify menu is shown by waiting for a menu item.
      WaitForShow(tab_groups::STGTabsMenuModel::kOpenGroup));
}

IN_PROC_BROWSER_TEST_F(ProjectsPanelTabGroupsInteractiveUiTest,
                       CreateNewTabGroupClosesPanelFirst) {
  RunTestSequence(WaitForShow(kVerticalTabStripTopContainerElementId),
                  AddTabGroup(u"Test Group"), OpenProjectsPanel(),
                  WaitForShow(kProjectsPanelNewTabGroupButtonElementId),
                  MoveMouseTo(kProjectsPanelNewTabGroupButtonElementId),
                  // Clicking the create new tab group button should close the
                  // panel.
                  ClickMouse(), WaitForHide(kProjectsPanelViewElementId));
}
