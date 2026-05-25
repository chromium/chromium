// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sessions/session_service_browser_helper.h"

#include "base/check_deref.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_contents.h"

namespace {

void UpdateTabGroupSessionMetadata(Profile& profile,
                                   TabStripModel& tab_strip_model,
                                   SessionID session_id,
                                   const tab_groups::TabGroupId& group_id) {
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(&profile);
  if (!session_service) {
    return;
  }

  const tab_groups::TabGroupVisualData* visual_data =
      tab_strip_model.group_model()->GetTabGroup(group_id)->visual_data();

  session_service->SetTabGroupMetadata(session_id, group_id, visual_data);
}

}  // namespace

SessionServiceBrowserHelper::SessionServiceBrowserHelper(
    TabStripModel* tab_strip_model,
    SessionID session_id,
    BrowserWindowInterface::Type browser_type,
    Profile* profile)
    : tab_strip_model_(CHECK_DEREF(tab_strip_model)),
      session_id_(session_id),
      browser_type_(browser_type),
      profile_(CHECK_DEREF(profile)) {
  tab_strip_model_->AddObserver(this);
}

SessionServiceBrowserHelper::~SessionServiceBrowserHelper() = default;

void SessionServiceBrowserHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        sessions::SessionTabHelper::FromWebContents(contents.contents)
            ->SetWindowID(session_id_);

        SyncHistoryWithTabs(contents.index);

        SessionServiceBase* service = GetSessionService();
        if (service) {
          service->TabInserted(contents.contents);
          int new_active_index = tab_strip_model->active_index();
          if (contents.index < new_active_index) {
            service->SetSelectedTabInWindow(session_id_, new_active_index);
          }
        }
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        SessionServiceBase* service = GetSessionService();
        if (service &&
            TabRemoveReasonUtils::WillDeleteTab(contents.remove_reason)) {
          service->TabClosing(contents.contents);
        }

        if (!tab_strip_model->closing_all()) {
          SessionServiceBase* service_existing = GetSessionServiceIfExisting();
          if (service_existing) {
            service_existing->SetSelectedTabInWindow(
                session_id_, tab_strip_model->active_index());
          }
          SyncHistoryWithTabs(0);
        }
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();
      SyncHistoryWithTabs(std::min(move->from_index, move->to_index));
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      SessionServiceBase* session_service = GetSessionService();
      if (session_service) {
        session_service->TabClosing(replace->old_contents);
      }

      // Insert new session logic:
      sessions::SessionTabHelper::FromWebContents(replace->new_contents)
          ->SetWindowID(session_id_);
      SyncHistoryWithTabs(replace->index);
      if (session_service) {
        session_service->TabInserted(replace->new_contents);
        int new_active_index = tab_strip_model->active_index();
        if (replace->index < new_active_index) {
          session_service->SetSelectedTabInWindow(session_id_,
                                                  new_active_index);
        }

        // The new_contents may end up with a different navigation stack. Force
        // the session service to update itself.
        session_service->TabRestored(
            replace->new_contents,
            tab_strip_model->IsTabPinned(replace->index));
      }
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (!selection.active_tab_changed()) {
    return;
  }

  // Update sessions (selected tab index and last active time). Don't force
  // creation of sessions. If sessions doesn't exist, the change will be picked
  // up by sessions when created.
  SessionServiceBase* service = GetSessionServiceIfExisting();
  if (service && !tab_strip_model->closing_all()) {
    service->SetSelectedTabInWindow(session_id_,
                                    tab_strip_model->active_index());
    service->SetLastActiveTime(
        session_id_,
        sessions::SessionTabHelper::IdForTab(selection.new_contents),
        base::Time::Now());
  }
}

void SessionServiceBrowserHelper::OnTabGroupChanged(
    const TabGroupChange& change) {
  // If apps ever get tab grouping, this function needs to be updated to
  // retrieve AppSessionService from the correct factory. Additionally,
  // AppSessionService doesn't support SetTabGroupMetadata, so some
  // work to refactor the code to support that into SessionServiceBase
  // would be the best way to achieve that.
  DCHECK(!IsRelevantToAppSessionService(browser_type_));
  DCHECK(tab_strip_model_->group_model());

  if (change.type == TabGroupChange::kVisualsChanged) {
    UpdateTabGroupSessionMetadata(*profile_, *tab_strip_model_, session_id_,
                                  change.group);
  } else if (change.type == TabGroupChange::kCreated &&
             change.GetCreateChange()->reason() ==
                 TabGroupChange::TabGroupCreationReason::
                     kInsertedFromAnotherTabstrip) {
    // When a detached group is inserted, we need to update the group of all the
    // corresponding detached tab in session service.
    for (tabs::TabInterface* tab :
         change.GetCreateChange()->GetDetachedTabs()) {
      UpdateTabGroupSessionDataForTab(tab, change.group);
    }
  } else if (change.type == TabGroupChange::kClosed &&
             change.GetCloseChange()->reason() ==
                 TabGroupChange::TabGroupClosureReason::kGroupClosed) {
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(&*profile_);
    // When a group is detached, we do not need to add the information for all
    // the detached tabs in tab restore service.
    if (tab_restore_service) {
      tab_restore_service->GroupClosed(change.group);
    }
  }
}

void SessionServiceBrowserHelper::OnTabPinnedStateChanged(
    tabs::TabInterface* tab,
    int index) {
  // See comment in `OnTabGroupChanged`.
  DCHECK(!IsRelevantToAppSessionService(browser_type_));
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(&*profile_);
  if (session_service) {
    session_service->SetPinnedState(
        session_id_, sessions::SessionTabHelper::IdForTab(tab->GetContents()),
        tab->IsPinned());
  }
}

void SessionServiceBrowserHelper::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  UpdateTabGroupSessionDataForTab(tab, new_group);
}

void SessionServiceBrowserHelper::OnSplitTabChanged(
    const SplitTabChange& change) {
  switch (change.type) {
    case SplitTabChange::Type::kAdded: {
      for (std::pair<tabs::TabInterface*, int> split_tabs :
           change.GetAddedChange()->tabs()) {
        UpdateSplitTabSessionData(split_tabs.first, change.split_id);
      }

      UpdateSplitTabSessionVisualData(change.split_id);
      break;
    }

    case SplitTabChange::Type::kVisualsChanged: {
      // Intermediate ratio updates from dragging shouldn't spam the session
      // storage. They are saved when the drag completes.
      if (!change.GetVisualsChange()->is_intermediate()) {
        UpdateSplitTabSessionVisualData(change.split_id);
      }
      break;
    }

    case SplitTabChange::Type::kContentsChanged: {
      // No need to do anything here since split is still present and no visual
      // information changed.
      break;
    }

    case SplitTabChange::Type::kRemoved: {
      for (std::pair<tabs::TabInterface*, int> split_tabs :
           change.GetRemovedChange()->tabs()) {
        UpdateSplitTabSessionData(split_tabs.first, std::nullopt);
      }
      break;
    }
  }
}

void SessionServiceBrowserHelper::SyncHistoryWithTabs(int index) {
  SessionServiceBase* service = GetSessionService();

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(&*profile_);

  if (!service && !session_service) {
    return;
  }

  if (index >= tab_strip_model_->count()) {
    return;
  }

  int current_index = index;
  for (tabs::TabCollection::TabIterator it(
           tab_strip_model_->GetTabAtIndex(index));
       it != tab_strip_model_->end(); ++it) {
    content::WebContents* web_contents = it->GetContents();
    if (web_contents) {
      SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
      if (service) {
        service->SetPinnedState(session_id_, tab_id, it->IsPinned());
      }

      if (!IsRelevantToAppSessionService(browser_type_) && session_service) {
        session_service->SetTabIndexInWindow(session_id_, tab_id,
                                             current_index);

        std::optional<tab_groups::TabGroupId> group_id =
            tab_strip_model_->GetTabGroupForTab(current_index);
        session_service->SetTabGroup(session_id_, tab_id, std::move(group_id));
      }
    }
    current_index++;
  }
}

void SessionServiceBrowserHelper::UpdateTabGroupSessionDataForTab(
    tabs::TabInterface* tab,
    std::optional<tab_groups::TabGroupId> group) {
  // See comment in `OnTabGroupChanged`.
  DCHECK(!IsRelevantToAppSessionService(browser_type_));
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(&*profile_);
  if (!session_service) {
    return;
  }

  session_service->SetTabGroup(
      session_id_, sessions::SessionTabHelper::IdForTab(tab->GetContents()),
      std::move(group));
}

void SessionServiceBrowserHelper::UpdateSplitTabSessionData(
    tabs::TabInterface* tab,
    std::optional<split_tabs::SplitTabId> split_id) {
  DCHECK(!IsRelevantToAppSessionService(browser_type_));
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(&*profile_);
  if (!session_service) {
    return;
  }

  session_service->SetSplitTab(
      session_id_, sessions::SessionTabHelper::IdForTab(tab->GetContents()),
      std::move(split_id));
}

void SessionServiceBrowserHelper::UpdateSplitTabSessionVisualData(
    const split_tabs::SplitTabId& split_id) {
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(&*profile_);
  if (!session_service) {
    return;
  }

  const split_tabs::SplitTabVisualData* visual_data =
      tab_strip_model_->GetSplitData(split_id)->visual_data();
  session_service->SetSplitTabData(session_id_, split_id, visual_data);
}

SessionServiceBase* SessionServiceBrowserHelper::GetSessionService() {
  if (IsRelevantToAppSessionService(browser_type_)) {
    return AppSessionServiceFactory::GetForProfile(&*profile_);
  }
  return SessionServiceFactory::GetForProfile(&*profile_);
}

SessionServiceBase* SessionServiceBrowserHelper::GetSessionServiceIfExisting() {
  if (IsRelevantToAppSessionService(browser_type_)) {
    return AppSessionServiceFactory::GetForProfileIfExisting(&*profile_);
  }
  return SessionServiceFactory::GetForProfileIfExisting(&*profile_);
}
