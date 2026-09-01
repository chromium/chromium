// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/chrome_switches.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"
#include "chrome/common/chrome_features.h"
#endif

namespace {

void TabGroupsDialogTimingToSource(
    base::OnceCallback<void(CloseTabSource)> callback,
    CloseTabSource source,
    tab_groups::DeletionDialogController::DeletionDialogTiming timing) {
  switch (timing) {
    case tab_groups::DeletionDialogController::DeletionDialogTiming::
        Synchronous: {
      std::move(callback).Run(source);
      return;
    }
    case tab_groups::DeletionDialogController::DeletionDialogTiming::
        Asynchronous: {
      std::move(callback).Run(CloseTabSource::kFromNonUIEvent);
      return;
    }
  }
}

}  // namespace

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, public:

BrowserTabStripModelDelegate::BrowserTabStripModelDelegate(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

BrowserTabStripModelDelegate::~BrowserTabStripModelDelegate() = default;

void BrowserTabStripModelDelegate::GlicUnpinTabsFromAllConversations(
    base::span<const tabs::TabHandle> tab_handles) {
  auto* service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser_->GetProfile());
  service->instance_coordinator().UnpinTabsFromAllInstances(
      tab_handles, glic::GlicUnpinTrigger::kContextMenu);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, TabStripModelDelegate implementation:

void BrowserTabStripModelDelegate::AddTabAt(
    const GURL& url,
    int index,
    bool foreground,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  chrome::AddTabAt(browser_, url, index, foreground, group, pinned);
}

BrowserWindowInterface* BrowserTabStripModelDelegate::CreateNewStripWithTabs(
    std::vector<NewStripContents> tabs,
    const gfx::Rect& window_bounds,
    bool maximize) {
  DCHECK(WindowFeatureController::From(browser_)->CanSupportWindowFeature(
      WindowFeatureController::WindowFeature::kFeatureTabStrip));

  // Create an empty new browser window the same size as the old one.
  BrowserWindowCreateParams params(browser_->GetProfile(), true);
  params.initial_bounds = window_bounds;
  params.initial_show_state = maximize ? ui::mojom::WindowShowState::kMaximized
                                       : ui::mojom::WindowShowState::kNormal;
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  TabStripModel* new_model = browser->GetTabStripModel();

  for (size_t i = 0; i < tabs.size(); ++i) {
    NewStripContents item = std::move(tabs[i]);

    // Enforce that there is an active tab in the strip at all times by forcing
    // the first web contents to be marked as active.
    if (i == 0) {
      item.add_types |= AddTabTypes::ADD_ACTIVE;
    }

    content::WebContents* const raw_web_contents =
        item.tab.get()->GetContents();
    new_model->InsertDetachedTabAt(static_cast<int>(i), std::move(item.tab),
                                   item.add_types);
    // Make sure the loading state is updated correctly, otherwise the throbber
    // won't start if the page is loading.
    // TODO(beng): find a better way of doing this.
    BrowserWebContentsDelegate::From(browser)->LoadingStateChanged(
        raw_web_contents, true);
  }

  return browser;
}

void BrowserTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  TabHelpers::AttachTabHelpers(contents);
}

int BrowserTabStripModelDelegate::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION |
         (browser_->GetTabStripModel()->count() > 1
              ? TabStripModelDelegate::TAB_MOVE_ACTION
              : 0);
}

bool BrowserTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return CanDuplicateTabAt(browser_, index);
}

bool BrowserTabStripModelDelegate::IsTabStripEditable() {
  return BrowserWindow::FromBrowser(browser_)->IsTabStripEditable();
}

content::WebContents* BrowserTabStripModelDelegate::DuplicateContentsAt(
    int index) {
  return DuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::DuplicateSplit(
    split_tabs::SplitTabId split) {
  chrome::DuplicateSplit(browser_, split);
}

void BrowserTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {
  std::vector<BrowserWindowInterface*> existing_browsers =
      TabMenuModelDelegate::From(browser_)->GetOtherBrowserWindows(
          web_app::AppBrowserController::IsWebApp(browser_));
  size_t existing_browser_count = existing_browsers.size();
  if (static_cast<size_t>(browser_index) < existing_browser_count &&
      existing_browsers[browser_index]) {
    chrome::MoveTabsToExistingWindow(browser_, existing_browsers[browser_index],
                                     indices);
  }
}

bool BrowserTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return CanMoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {
  // chrome:: to disambiguate the free function from this method.
  chrome::MoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {
  TabGroupModel* group_model = browser_->GetTabStripModel()->group_model();
  if (!group_model || !group_model->ContainsTabGroup(group)) {
    return;
  }

  chrome::MoveGroupToNewWindow(browser_, group);
}

std::optional<SessionID> BrowserTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  if (!BrowserSupportsHistoricalEntries()) {
    return std::nullopt;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());

  // We only create historical tab entries for tabbed browser windows.
  if (service &&
      WindowFeatureController::From(browser_)->CanSupportWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTabStrip)) {
    return service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetOrCreateForWebContents(contents),
        browser_->GetTabStripModel()->GetIndexOfWebContents(contents));
  }
  return std::nullopt;
}

void BrowserTabStripModelDelegate::CreateHistoricalGroup(
    const tab_groups::TabGroupId& group) {
  if (!BrowserSupportsHistoricalEntries()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->CreateHistoricalGroup(BrowserLiveTabContext::FindContextWithGroup(
                                       group, browser_->GetProfile()),
                                   group);
  }
}

void BrowserTabStripModelDelegate::CreateHistoricalSplit(
    const split_tabs::SplitTabId& split_id) {
  if (!BrowserSupportsHistoricalEntries()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->CreateHistoricalSplit(BrowserLiveTabContext::From(browser_),
                                   split_id);
  }
}

void BrowserTabStripModelDelegate::GroupAdded(
    const tab_groups::TabGroupId& group) {}

void BrowserTabStripModelDelegate::WillCloseGroup(
    const tab_groups::TabGroupId& group) {
  // Store updated information about the tab group in TabRestore.
  CreateHistoricalGroup(group);
}

void BrowserTabStripModelDelegate::WillCloseSplit(
    const split_tabs::SplitTabId& split_id) {
  CreateHistoricalSplit(split_id);
}

void BrowserTabStripModelDelegate::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->GroupCloseStopped(group);
  }
}

void BrowserTabStripModelDelegate::SplitClosed(
    const split_tabs::SplitTabId& split_id) {
  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->SplitClosed(split_id);
  }
}

void BrowserTabStripModelDelegate::SplitCloseStopped(
    const split_tabs::SplitTabId& split_id) {
  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->SplitCloseStopped(split_id);
  }
}

bool BrowserTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return UnloadController::From(browser_)->RunUnloadListenerBeforeClosing(
      contents);
}

bool BrowserTabStripModelDelegate::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return UnloadController::From(browser_)->ShouldRunUnloadListenerBeforeClosing(
      contents);
}

bool BrowserTabStripModelDelegate::CanReload() const {
  return chrome::CanReload(browser_);
}

void BrowserTabStripModelDelegate::AddToReadLater(
    std::vector<content::WebContents*> web_contentses) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!model || !model->loaded()) {
    return;
  }

  chrome::MoveTabsToReadLater(browser_, web_contentses);
}

bool BrowserTabStripModelDelegate::SupportsReadLater() {
  return !browser_->GetProfile()->IsGuestSession() && !IsForWebApp();
}

bool BrowserTabStripModelDelegate::IsForWebApp() {
  return web_app::AppBrowserController::IsWebApp(browser_);
}

void BrowserTabStripModelDelegate::CopyURL(content::WebContents* web_contents) {
  chrome::CopyURL(browser_, web_contents);
}

void BrowserTabStripModelDelegate::GoBack(content::WebContents* web_contents) {
  chrome::GoBack(web_contents);
}

bool BrowserTabStripModelDelegate::CanGoBack(
    content::WebContents* web_contents) {
  return chrome::CanGoBack(web_contents);
}

bool BrowserTabStripModelDelegate::IsNormalWindow() {
  return browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
}

BrowserWindowInterface*
BrowserTabStripModelDelegate::GetBrowserWindowInterface() {
  return browser_;
}

void BrowserTabStripModelDelegate::NewSplitTab(
    std::vector<int> indices,
    split_tabs::SplitTabLayout layout,
    split_tabs::SplitTabCreatedSource source) {
  if (indices.empty()) {
    chrome::NewSplitTab(browser_, layout, source);
  } else {
    browser_->GetTabStripModel()->AddToNewSplit(
        indices, split_tabs::SplitTabVisualData(layout), source);
  }
}

void BrowserTabStripModelDelegate::OnGroupsDestruction(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> close_callback,
    bool delete_groups) {
  if (!delete_groups) {
    // Close the groups rather than delete them to retain the saved group.
    for (auto group_id : group_ids) {
      tab_groups::SavedTabGroupUtils::RemoveGroupFromTabstrip(browser_,
                                                              group_id);
    }
    std::move(close_callback).Run();
  } else {
    tab_groups::SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
        browser_, tab_groups::GroupDeletionReason::ClosedLastTab, group_ids,
        base::IgnoreArgs<
            tab_groups::DeletionDialogController::DeletionDialogTiming>(
            std::move(close_callback)));
  }
}

void BrowserTabStripModelDelegate::OnRemovingAllTabsFromGroups(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> callback) {
  tab_groups::SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
      browser_, tab_groups::GroupDeletionReason::UngroupedLastTab, group_ids,
      base::IgnoreArgs<
          tab_groups::DeletionDialogController::DeletionDialogTiming>(
          std::move(callback)));
}

void BrowserTabStripModelDelegate::CloseTab(
    const tabs::TabInterface* tab_interface,
    CloseTabSource source,
    base::OnceCallback<void(CloseTabSource)> on_approved) {
  TabStripModel* model = browser_->GetTabStripModel();
  std::optional<int> maybe_tab_index = model->GetIndexOfTab(tab_interface);
  if (!maybe_tab_index.has_value()) {
    return;
  }
  int tab_index = maybe_tab_index.value();

  if (!web_app::IsTabClosable(model, tab_index)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Tabs cannot be closed when the app is in locked fullscreen, which is
  // available only on ChromeOS.
  if (features::IsUseUnifiedLockedStateControllerEnabled()) {
    if (!chromeos::LockedStateController::From(browser_)
             ->GetCapabilities()
             .allow_tab_modification) {
      return;
    }
  } else if (platform_util::IsBrowserLockedFullscreen(browser_)) {
    return;
  }
#endif

  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksCloseTabExpandsSidePanel)) {
    ContextualTasksCloseButtonController* const close_button_controller =
        ContextualTasksCloseButtonController::From(browser_);
    if (tab_interface && tab_interface->IsActivated() &&
        close_button_controller &&
        close_button_controller->ShouldShowCloseButton()) {
      close_button_controller->MaybeCloseTabExpandSidePanel();
      return;
    }
  }

  auto [cb1, cb2] = base::SplitOnceCallback(std::move(on_approved));

  // Only consider pausing the close operation if this is the last remaining
  // tab (since otherwise closing it won't close the browser window).
  if (model->count() <= 1) {
    // Closing this tab will close the current window. See if the browser wants
    // to prompt the user before the browser is allowed to close.
    const UnloadController::WarnBeforeClosingResult result =
        UnloadController::From(browser_)->MaybeWarnBeforeClosing(base::BindOnce(
            [](base::WeakPtr<BrowserTabStripModelDelegate> delegate,
               const tabs::TabInterface* tab,
               base::OnceCallback<void(CloseTabSource)> cb,
               UnloadController::WarnBeforeClosingResult result) {
              if (delegate &&
                  result ==
                      UnloadController::WarnBeforeClosingResult::kOkToClose) {
                delegate->CloseTab(tab, CloseTabSource::kFromNonUIEvent,
                                   std::move(cb));
              }
            },
            weak_factory_.GetWeakPtr(), tab_interface, std::move(cb1)));

    if (result != UnloadController::WarnBeforeClosingResult::kOkToClose) {
      return;
    }
  }

  // Check to make sure the tab is not the last in its group.
  std::vector<tab_groups::TabGroupId> groups_to_delete =
      model->GetGroupsDestroyedFromRemovingIndices({tab_index});

  auto do_close = base::BindOnce(
      [](base::WeakPtr<BrowserTabStripModelDelegate> delegate,
         base::WeakPtr<content::WebContents> web_contents,
         base::OnceCallback<void(CloseTabSource)> on_approved,
         CloseTabSource source) {
        if (!delegate) {
          return;
        }
        BrowserWindowInterface* browser = delegate->browser_;
        TabStripModel* model = browser->GetTabStripModel();

        if (on_approved) {
          std::move(on_approved).Run(source);
        }

        if (!web_contents) {
          return;
        }
        int tab_index = model->GetIndexOfWebContents(web_contents.get());
        if (tab_index == TabStripModel::kNoTab) {
          return;
        }

        const tabs::TabInterface* tab = model->GetTabAtIndex(tab_index);

        if (tab->GetGroup().has_value()) {
          base::RecordAction(base::UserMetricsAction("CloseGroupedTab"));

          if (model->count() == 1) {
            // Prevent the browser from closing when the last grouped tab is
            // closed from the browser by adding a new tab.
            chrome::NewTab(browser, NewTabTypes::kNoUserAction);
            // In some situations the new tab is assigned a group. So if it is
            // in a group, we remove it from the group so that after closing the
            // tab at `tab_index`, the browser shows a tab without a group.
            model->RemoveFromGroup({1});
          }
        }

        model->CloseWebContents(web_contents.get(),
                                TabCloseTypes::CLOSE_USER_GESTURE |
                                    TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);

        // Try to show reading list IPH if needed.
        if (model->count() >= 7) {
          BrowserUserEducationInterface::From(browser)->MaybeShowFeaturePromo(
              feature_engagement::kIPHReadingListEntryPointFeature);
        }
      },
      weak_factory_.GetWeakPtr(), tab_interface->GetContents()->GetWeakPtr(),
      std::move(cb2));

  if (groups_to_delete.empty()) {
    std::move(do_close).Run(source);
    return;
  }

  auto timing_mapped_callback = base::BindOnce(&TabGroupsDialogTimingToSource,
                                               std::move(do_close), source);

  // If the user is destroying the last tab in a saved or shared group via the
  // tabstrip, a dialog is shown that will decide whether to destroy the tab or
  // not. It will first ungroup the tab, then close the tab.
  tab_groups::SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
      browser_, tab_groups::GroupDeletionReason::ClosedLastTab,
      groups_to_delete, std::move(timing_mapped_callback));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, private:

void BrowserTabStripModelDelegate::CloseFrame() {
  browser_->GetWindow()->Close();
}

bool BrowserTabStripModelDelegate::BrowserSupportsHistoricalEntries() {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  return browser_->GetProfile() && !browser_->GetProfile()->IsOffTheRecord();
}

}  // namespace chrome
