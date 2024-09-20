// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_helper.h"

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/values.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace sessions {
namespace {

// Specifies what entries are added.
enum class AddBehavior {
  // Adds the current entry, and entries preceeding it.
  kCurrentAndPreceedingEntries,

  // Adds entries after the current.
  kEntriesFollowingCurrentEntry,
};

// Adds serialized navigation entries from a LiveTab.
void AddSerializedNavigationEntries(
    LiveTab* live_tab,
    AddBehavior behavior,
    std::vector<SerializedNavigationEntry>& navigations) {
  // It is assumed this is called for back navigations first, at which point the
  // vector should be empty. This is necessary as back navigations are added
  // in reverse order and then the vector is reversed.
  DCHECK(behavior == AddBehavior::kEntriesFollowingCurrentEntry ||
         navigations.empty());
  const int max_index = live_tab->GetEntryCount();
  const int delta =
      (behavior == AddBehavior::kCurrentAndPreceedingEntries) ? -1 : 1;
  int current_index = live_tab->GetCurrentEntryIndex();
  if (behavior == AddBehavior::kEntriesFollowingCurrentEntry)
    ++current_index;
  int added_count = 0;
  while (current_index >= 0 && current_index < max_index &&
         added_count <= gMaxPersistNavigationCount) {
    SerializedNavigationEntry entry = live_tab->GetEntryAtIndex(current_index);
    current_index += delta;
    // Reader Mode is meant to be considered a "mode" that users can only enter
    // using a button in the omnibox, so it does not show up in recently closed
    // tabs, session sync, or chrome://history. Remove Reader Mode pages from
    // the navigations.
    if (entry.virtual_url().SchemeIs(dom_distiller::kDomDistillerScheme)) {
      continue;
    }

    // An entry might have an empty URL (e.g. if it's the initial
    // NavigationEntry). Don't try to persist it, as it is not actually
    // associated with any navigation and will just result in about:blank on
    // session restore.
    if (entry.virtual_url().is_empty()) {
      continue;
    }

    // As this code was identified as doing a lot of allocations, push_back is
    // always used and the vector is reversed for `kCurrentAndPreceedingEntries`
    // when done. Doing this instead of inserting at the beginning results in
    // less memory operations.
    navigations.push_back(std::move(entry));
    ++added_count;
  }
  // Iteration for `kCurrentAndPreceedingEntries` happens in descending order.
  // This results in the entries being added in reverse order. Use
  // std::reverse() so the entries end up in ascending order.
  if (behavior == AddBehavior::kCurrentAndPreceedingEntries) {
    std::reverse(navigations.begin(), navigations.end());
  }
}

}  // namespace

// TabRestoreServiceHelper::Observer -------------------------------------------

TabRestoreServiceHelper::Observer::~Observer() {}

void TabRestoreServiceHelper::Observer::OnClearEntries() {}

void TabRestoreServiceHelper::Observer::OnNavigationEntriesDeleted() {}

void TabRestoreServiceHelper::Observer::OnRestoreEntryById(
    SessionID id,
    Entries::const_iterator entry_iterator) {}

void TabRestoreServiceHelper::Observer::OnAddEntry() {}

// TabRestoreServiceHelper -----------------------------------------------------

TabRestoreServiceHelper::TabRestoreServiceHelper(
    TabRestoreService* tab_restore_service,
    TabRestoreServiceClient* client,
    tab_restore::TimeFactory* time_factory)
    : tab_restore_service_(tab_restore_service),
      observer_(nullptr),
      client_(client),
      restoring_(false),
      time_factory_(time_factory) {
  DCHECK(tab_restore_service_);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "TabRestoreServiceHelper",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void TabRestoreServiceHelper::SetHelperObserver(Observer* observer) {
  observer_ = observer;
}

TabRestoreServiceHelper::~TabRestoreServiceHelper() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceDestroyed(tab_restore_service_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void TabRestoreServiceHelper::AddObserver(TabRestoreServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreServiceHelper::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::optional<SessionID> TabRestoreServiceHelper::CreateHistoricalTab(
    LiveTab* live_tab,
    int index) {
  if (restoring_) {
    return std::nullopt;
  }

  // If an entire window or group is being closed than all of the tabs have
  // already been persisted via "BrowserClosing" or "CreateHistoricalGroup".
  // Ignore the subsequent tab closing notifications.
  LiveTabContext* context = client_->FindLiveTabContextForTab(live_tab);
  if (closing_contexts_.find(context) != closing_contexts_.end()) {
    return std::nullopt;
  }
  std::optional<tab_groups::TabGroupId> group =
      context ? context->GetTabGroupForTab(index) : std::nullopt;
  if (group.has_value() &&
      closing_groups_.find(group.value()) != closing_groups_.end()) {
    return std::nullopt;
  }

  // Save the Window as well as the Tab if this is the last tab of an app
  // browser to ensure the tab will reopen in the correct app window.
  if (context && context->GetTabCount() == 1 &&
      !context->GetAppName().empty()) {
    BrowserClosing(context);
    return std::nullopt;
  }

  auto local_tab = std::make_unique<Tab>();
  PopulateTab(local_tab.get(), index, context, live_tab);
  if (local_tab->navigations.empty())
    return std::nullopt;

  SessionID id = local_tab->id;
  AddEntry(std::move(local_tab), true, true);
  return id;
}

void TabRestoreServiceHelper::BrowserClosing(LiveTabContext* context) {
  closing_contexts_.insert(context);

  auto window = std::make_unique<Window>();
  window->type = context->GetWindowType();
  window->selected_tab_index = context->GetSelectedIndex();
  window->timestamp = TimeNow();
  window->app_name = context->GetAppName();
  window->bounds = context->GetRestoredBounds();
  window->show_state = context->GetRestoredState();
  window->workspace = context->GetWorkspace();
  window->user_title = context->GetUserTitle();
  window->extra_data = context->GetExtraDataForWindow();

  for (int tab_index = 0; tab_index < context->GetTabCount(); ++tab_index) {
    auto tab = std::make_unique<Tab>();
    PopulateTab(tab.get(), tab_index, context,
                context->GetLiveTabAt(tab_index));
    if (tab->navigations.empty()) {
      continue;
    }
    tab->browser_id = context->GetSessionID().id();

    if (tab->group.has_value() &&
        !window->tab_groups.contains(tab->group.value())) {
      // Add new groups to the mapping if we haven't already.
      window->tab_groups.emplace(tab->group.value(),
                                 Group::FromTab(*tab.get()));
    }

    window->tabs.push_back(std::move(tab));
  }

  if (window->tabs.size() == 1 && window->app_name.empty() &&
      window->user_title.empty()) {
    // Short-circuit creating a Window if only 1 tab was present. This fixes
    // http://crbug.com/56744.
    AddEntry(std::move(window->tabs[0]), true, true);
  } else if (!window->tabs.empty()) {
    window->selected_tab_index = std::min(
        static_cast<int>(window->tabs.size() - 1), window->selected_tab_index);
    AddEntry(std::move(window), true, true);
  }
}

void TabRestoreServiceHelper::BrowserClosed(LiveTabContext* context) {
  closing_contexts_.erase(context);
}

std::unique_ptr<tab_restore::Group>
TabRestoreServiceHelper::CreateHistoricalGroupImpl(
    LiveTabContext* context,
    const tab_groups::TabGroupId& id) {
  auto group = std::make_unique<Group>();
  group->group_id = id;
  group->saved_group_id = context->GetSavedTabGroupIdForGroup(id);
  group->visual_data = *context->GetVisualDataForGroup(id);
  group->browser_id = context->GetSessionID().id();
  group->timestamp = TimeNow();

  for (int tab_index = 0; tab_index < context->GetTabCount(); ++tab_index) {
    if (context->GetTabGroupForTab(tab_index) == id) {
      auto tab = std::make_unique<Tab>();
      PopulateTab(tab.get(), tab_index, context,
                  context->GetLiveTabAt(tab_index));
      if (!tab->navigations.empty()) {
        tab->browser_id = context->GetSessionID().id();
        group->tabs.push_back(std::move(tab));
      }
    }
  }

  return group;
}

void TabRestoreServiceHelper::CreateHistoricalGroup(
    LiveTabContext* context,
    const tab_groups::TabGroupId& id) {
  closing_groups_.insert(id);

  auto group = CreateHistoricalGroupImpl(context, id);
  if (!group->tabs.empty()) {
    AddEntry(std::move(group), true, true);
  }
}

void TabRestoreServiceHelper::GroupClosed(const tab_groups::TabGroupId& group) {
  closing_groups_.erase(group);
}

void TabRestoreServiceHelper::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  // TODO(crbug.com/40750891): Delete this function if the group entry was never
  // created, or adjust the group entry here to account for any unclosed tabs.

  closing_groups_.erase(group);
}

void TabRestoreServiceHelper::ClearEntries() {
  if (observer_) {
    observer_->OnClearEntries();
  }
  entries_.clear();
  NotifyEntriesChanged();
}

bool TabRestoreServiceHelper::DeleteFromTab(const DeletionPredicate& predicate,
                                            Tab* tab) {
  // TODO(dullweber): Change to DCHECK() when this is tested to be true.
  CHECK(ValidateTab(*tab));
  std::vector<SerializedNavigationEntry> new_navigations;
  int deleted_navigations_count = 0;
  for (size_t i = 0; i < tab->navigations.size(); i++) {
    SerializedNavigationEntry& navigation = tab->navigations[i];
    if (predicate.Run(navigation)) {
      // If the current navigation is deleted, remove this tab.
      if (static_cast<int>(i) == tab->current_navigation_index) {
        return true;
      }
      deleted_navigations_count++;
    } else {
      // Adjust indices according to number of deleted navigations.
      if (static_cast<int>(i) == tab->current_navigation_index) {
        tab->current_navigation_index -= deleted_navigations_count;
      }
      DCHECK_GE(navigation.index(), deleted_navigations_count);
      navigation.set_index(navigation.index() - deleted_navigations_count);
      new_navigations.push_back(std::move(navigation));
    }
  }
  tab->navigations = std::move(new_navigations);
  // TODO(dullweber): Change to DCHECK() when this is tested to be true.
  CHECK(tab->navigations.empty() || ValidateTab(*tab));
  return tab->navigations.empty();
}

bool TabRestoreServiceHelper::DeleteFromWindow(
    const DeletionPredicate& predicate,
    Window* window) {
  // TODO(dullweber): Change to DCHECK() when this is tested to be true.
  CHECK(ValidateWindow(*window));
  std::vector<std::unique_ptr<Tab>> new_tabs;
  int deleted_tabs_count = 0;
  for (size_t i = 0; i < window->tabs.size(); i++) {
    std::unique_ptr<Tab>& tab = window->tabs[i];
    if (DeleteFromTab(predicate, tab.get())) {
      if (static_cast<int>(i) == window->selected_tab_index) {
        window->selected_tab_index = new_tabs.empty() ? 0 : new_tabs.size() - 1;
      }
      deleted_tabs_count++;
    } else {
      // Adjust indices according to number of deleted tabs.
      if (static_cast<int>(i) == window->selected_tab_index) {
        window->selected_tab_index -= deleted_tabs_count;
      }
      if (tab->tabstrip_index >= 0) {
        DCHECK_GE(tab->tabstrip_index, deleted_tabs_count);
        tab->tabstrip_index -= deleted_tabs_count;
      }
      new_tabs.push_back(std::move(tab));
    }
  }
  window->tabs = std::move(new_tabs);
  // TODO(dullweber): Change to DCHECK() when this is tested to be true.
  CHECK(window->tabs.empty() || ValidateWindow(*window));
  return window->tabs.empty();
}

bool TabRestoreServiceHelper::DeleteFromGroup(
    const DeletionPredicate& predicate,
    Group* group) {
  std::vector<std::unique_ptr<Tab>> new_tabs;
  int deleted_tabs_count = 0;
  for (std::unique_ptr<Tab>& tab : group->tabs) {
    if (DeleteFromTab(predicate, tab.get())) {
      deleted_tabs_count++;
    } else {
      // Adjust indices according to number of deleted tabs.
      if (tab->tabstrip_index >= 0) {
        DCHECK_GE(tab->tabstrip_index, deleted_tabs_count);
        tab->tabstrip_index -= deleted_tabs_count;
      }
      new_tabs.push_back(std::move(tab));
    }
  }
  group->tabs = std::move(new_tabs);
  return group->tabs.empty();
}

void TabRestoreServiceHelper::DeleteNavigationEntries(
    const DeletionPredicate& predicate) {
  Entries new_entries;
  for (std::unique_ptr<Entry>& entry : entries_) {
    switch (entry->type) {
      case tab_restore::Type::TAB: {
        Tab* tab = static_cast<Tab*>(entry.get());
        if (!DeleteFromTab(predicate, tab)) {
          new_entries.push_back(std::move(entry));
        }
        break;
      }
      case tab_restore::Type::WINDOW: {
        Window* window = static_cast<Window*>(entry.get());
        if (!DeleteFromWindow(predicate, window)) {
          // If only a single tab is left, just keep the tab.
          if (window->tabs.size() == 1u) {
            new_entries.push_back(std::move(window->tabs.front()));
          } else {
            new_entries.push_back(std::move(entry));
          }
        }
        break;
      }
      case tab_restore::Type::GROUP: {
        Group* group = static_cast<Group*>(entry.get());
        if (!DeleteFromGroup(predicate, group)) {
          new_entries.push_back(std::move(entry));
        }
        break;
      }
    }
  }
  entries_ = std::move(new_entries);
  if (observer_) {
    observer_->OnNavigationEntriesDeleted();
  }
  NotifyEntriesChanged();
}

const TabRestoreService::Entries& TabRestoreServiceHelper::entries() const {
  return entries_;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreMostRecentEntry(
    LiveTabContext* context) {
  if (entries_.empty()) {
    return std::vector<LiveTab*>();
  }
  return RestoreEntryById(context, entries_.front()->id,
                          WindowOpenDisposition::UNKNOWN);
}

void TabRestoreServiceHelper::RemoveEntryById(SessionID id) {
  auto it = GetEntryIteratorById(id);
  if (it == entries_.end()) {
    return;
  }

  entries_.erase(it);
  NotifyEntriesChanged();
}

LiveTabContext* TabRestoreServiceHelper::RestoreTabOrGroupFromWindow(
    Window& window,
    SessionID id,
    LiveTabContext* context,
    WindowOpenDisposition disposition,
    std::vector<LiveTab*>* live_tabs) {
  // Restore a single tab from the window. Find the tab that matches the
  // ID in the window and restore it.
  bool found_tab_to_delete = false;
  SessionID::id_type restored_tab_browser_id;
  for (size_t tab_i = 0; tab_i < window.tabs.size() && !found_tab_to_delete;
       tab_i++) {
    const Tab& tab = *window.tabs[tab_i];
    if (tab.id != id && tab.original_id != id) {
      continue;
    }

    // Restore the tab.
    restored_tab_browser_id = tab.browser_id;
    LiveTab* restored_tab = nullptr;
    context = RestoreTab(tab, context, disposition,
                         sessions::tab_restore::WINDOW, &restored_tab);
    live_tabs->push_back(restored_tab);

    std::optional<tab_groups::TabGroupId> group_id = tab.group;
    window.tabs.erase(window.tabs.begin() + tab_i);

    if (group_id.has_value()) {
      auto other_tabs_in_group = std::find_if(
          window.tabs.begin(), window.tabs.end(), [&group_id](const auto& tab) {
            return tab->group.has_value() && tab->group.value() == group_id;
          });

      if (other_tabs_in_group == window.tabs.end()) {
        window.tab_groups.erase(group_id.value());
      }
    }

    if (!window.tabs.empty()) {
      // Adjust |selected_tab index| to keep the window in a valid state.
      if (static_cast<int>(tab_i) <= window.selected_tab_index) {
        window.selected_tab_index = std::max(0, window.selected_tab_index - 1);
      }
    }

    found_tab_to_delete = true;
  }

  // Check the groups for the id if we didn't find the tab.
  if (!found_tab_to_delete) {
    for (auto& group_pair : window.tab_groups) {
      auto& group = group_pair.second;
      if (group->id != id && group->original_id != id) {
        continue;
      }

      restored_tab_browser_id = group->browser_id;
      tab_groups::TabGroupId group_id = group->group_id;

      // Restore tabs in the window that belong to `group_id`.
      for (size_t tab_i = 0; tab_i < window.tabs.size();) {
        const Tab& tab = *window.tabs[tab_i];
        if (!tab.group.has_value() || tab.group.value() != group_id) {
          // Different group; Do nothing.
          tab_i++;
          continue;
        }

        // Restore the tab.
        LiveTab* restored_tab = nullptr;
        LiveTabContext* new_context =
            RestoreTab(tab, context, disposition, sessions::tab_restore::WINDOW,
                       &restored_tab);
        if (tab_i != 0) {
          // CHECK that the context should be the same except for the first tab.
          DCHECK_EQ(new_context, context);
        }
        context = new_context;
        live_tabs->push_back(restored_tab);

        window.tabs.erase(window.tabs.begin() + tab_i);
      }

      // Clear group from mapping.
      window.tab_groups.erase(group_id);

      if (!window.tabs.empty()) {
        // Adjust |selected_tab index| to keep the window in a valid state.
        if (window.selected_tab_index >= static_cast<int>(window.tabs.size())) {
          window.selected_tab_index =
              std::max(0, static_cast<int>(window.tabs.size() - 1));
        }
        UpdateTabBrowserIDs(restored_tab_browser_id, context->GetSessionID());
      }

      break;
    }
  }

  if (!window.tabs.empty()) {
    // Update the browser ID of the rest of the tabs in the window so if
    // any one is restored, it goes into the same window as the tab
    // being restored now.
    CHECK(ValidateWindow(window));
    UpdateTabBrowserIDs(restored_tab_browser_id, context->GetSessionID());
  }

  return context;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreEntryById(
    LiveTabContext* context,
    SessionID id,
    WindowOpenDisposition disposition) {
  auto entry_iterator = GetEntryIteratorById(id);
  if (entry_iterator == entries_.end()) {
    // Don't hoark here, we allow an invalid id.
    return std::vector<LiveTab*>();
  }

  if (observer_) {
    observer_->OnRestoreEntryById(id, entry_iterator);
  }
  restoring_ = true;
  auto& entry = **entry_iterator;

  // Normally an entry's ID should match the ID that is being restored. If it
  // does not, then the entry is a window, group, or group within a window. In
  // these cases either a single tab or an entire group could be restored.
  // Reachable through OS-level menus like Mac > History.
  bool entry_id_matches_restore_id = entry.id == id || entry.original_id == id;

  // |context| will be NULL in cases where one isn't already available (eg,
  // when invoked on Mac OS X with no windows open). In this case, create a
  // new browser into which we restore the tabs.
  std::vector<LiveTab*> live_tabs;
  switch (entry.type) {
    case tab_restore::Type::TAB: {
      auto& tab = static_cast<const Tab&>(entry);

      if (tab.timestamp != base::Time() &&
          !tab.timestamp.ToDeltaSinceWindowsEpoch().is_zero()) {
        UMA_HISTOGRAM_LONG_TIMES("TabRestore.Tab.TimeBetweenClosedAndRestored",
                                 TimeNow() - tab.timestamp);
      }

      LiveTab* restored_tab = nullptr;
      context =
          RestoreTab(tab, context, disposition, entry.type, &restored_tab);
      live_tabs.push_back(restored_tab);
      context->ShowBrowserWindow();
      break;
    }
    case tab_restore::Type::WINDOW: {
      LiveTabContext* current_context = context;
      auto& window = static_cast<Window&>(entry);

      if (window.timestamp != base::Time() &&
          !window.timestamp.ToDeltaSinceWindowsEpoch().is_zero()) {
        UMA_HISTOGRAM_LONG_TIMES(
            "TabRestore.Window.TimeBetweenClosedAndRestored",
            TimeNow() - window.timestamp);
      }

      // When restoring a window, either the entire window can be restored, or a
      // single tab within it. If the entry's ID matches the one to restore, or
      // the entry corresponds to an application, then the entire window will be
      // restored.
      if (entry_id_matches_restore_id || !window.app_name.empty()) {
        context = client_->CreateLiveTabContext(
            context, window.type, window.app_name, window.bounds,
            window.show_state, window.workspace, window.user_title,
            window.extra_data);

        CHECK(!window.tabs.empty());
        const int selected_tab_index =
            window.selected_tab_index >= 0 &&
                    window.selected_tab_index <
                        static_cast<int>(window.tabs.size())
                ? window.selected_tab_index
                : 0;
        const SessionID selected_tab_id = window.tabs[selected_tab_index]->id;

        for (const auto& tab : window.tabs) {
          const bool select_tab = tab->id == selected_tab_id;
          LiveTab* restored_tab = context->AddRestoredTab(
              *tab.get(), context->GetTabCount(), select_tab, entry.type);

          if (restored_tab) {
            client_->OnTabRestored(
                tab->navigations.at(tab->current_navigation_index)
                    .virtual_url());
            live_tabs.push_back(restored_tab);
          }
        }

        // Update all tabs to point to the correct context.
        if (auto browser_id = window.tabs[0]->browser_id) {
          UpdateTabBrowserIDs(browser_id, context->GetSessionID());
        }
      } else {
        // Restore a single entry from the window. It could be a single tab, or
        // an entire group.
        context = RestoreTabOrGroupFromWindow(window, id, context, disposition,
                                              &live_tabs);

        if (window.tabs.empty()) {
          // Remove the entry if there is nothing left to restore.
          entries_.erase(entry_iterator);
        }
      }

      context->ShowBrowserWindow();

      if (disposition == WindowOpenDisposition::CURRENT_TAB &&
          current_context && current_context->GetActiveLiveTab()) {
        current_context->CloseTab();
      }
      break;
    }
    case tab_restore::Type::GROUP: {
      auto& group = static_cast<Group&>(entry);

      if (group.timestamp != base::Time() &&
          !group.timestamp.ToDeltaSinceWindowsEpoch().is_zero()) {
        UMA_HISTOGRAM_LONG_TIMES(
            "TabRestore.Group.TimeBetweenClosedAndRestored",
            TimeNow() - group.timestamp);
      }

      // When restoring a group, either the entire group can be restored, or a
      // single tab within it. If the entry's ID matches the one to restore,
      // then the entire group will be restored.
      if (entry_id_matches_restore_id) {
        for (const auto& tab : group.tabs) {
          LiveTab* restored_tab = context->AddRestoredTab(
              *tab.get(), context->GetTabCount(), group.tabs[0]->id == tab->id,
              entry.type);
          live_tabs.push_back(restored_tab);
        }
      } else {
        // Restore a single tab from the group. Find the tab that matches the
        // ID in the group and restore it.
        for (size_t i = 0; i < group.tabs.size(); i++) {
          const Tab& tab = *group.tabs[i];
          if (tab.id == id) {
            LiveTab* restored_tab = nullptr;
            context = RestoreTab(tab, context, disposition, entry.type,
                                 &restored_tab);
            live_tabs.push_back(restored_tab);
            CHECK(ValidateGroup(group));
            group.tabs.erase(group.tabs.begin() + i);
            if (group.tabs.empty()) {
              entries_.erase(entry_iterator);
            }

            break;
          }
        }
      }

      context->ShowBrowserWindow();
      break;
    }
  }

  if (entry_id_matches_restore_id) {
    entries_.erase(entry_iterator);
  }

  restoring_ = false;
  NotifyEntriesChanged();
  return live_tabs;
}

bool TabRestoreServiceHelper::IsRestoring() const {
  return restoring_;
}

void TabRestoreServiceHelper::NotifyEntriesChanged() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceChanged(tab_restore_service_);
}

void TabRestoreServiceHelper::NotifyLoaded() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceLoaded(tab_restore_service_);
}

void TabRestoreServiceHelper::AddEntry(std::unique_ptr<Entry> entry,
                                       bool notify,
                                       bool to_front) {
  if (!FilterEntry(*entry) || (entries_.size() >= kMaxEntries && !to_front)) {
    return;
  }

  if (entry->type == sessions::tab_restore::WINDOW) {
    auto& window = static_cast<Window&>(*entry.get());
    if (window.tab_groups.empty()) {
      for (auto& tab : window.tabs) {
        if (tab->group.has_value() &&
            !window.tab_groups.contains(tab->group.value())) {
          // Creating the mapping here covers the cases where we close a browser
          // window and when restoring the last session on browser startup.
          auto group = Group::FromTab(*tab);
          window.tab_groups.emplace(group->group_id, std::move(group));
        }
      }
    }
  }

  if (to_front) {
    entries_.push_front(std::move(entry));
  } else {
    entries_.push_back(std::move(entry));
  }

  PruneEntries();

  if (notify) {
    NotifyEntriesChanged();
  }

  if (observer_) {
    observer_->OnAddEntry();
  }
}

void TabRestoreServiceHelper::PruneEntries() {
  Entries new_entries;

  for (auto& entry : entries_) {
    if (FilterEntry(*entry) && new_entries.size() < kMaxEntries) {
      new_entries.push_back(std::move(entry));
    }
  }

  entries_ = std::move(new_entries);
}

TabRestoreService::Entries::iterator
TabRestoreServiceHelper::GetEntryIteratorById(SessionID id) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    // Check if the current entry matches |id|. This can be a Tab, Group, or
    // Window.
    if ((*i)->id == id || (*i)->original_id == id) {
      return i;
    }

    if ((*i)->type == tab_restore::Type::WINDOW) {
      // Check if `id` matches a tab...
      const auto& window = static_cast<const Window&>(**i);
      for (const auto& tab : window.tabs) {
        if (tab->id == id || tab->original_id == id) {
          return i;
        }
      }

      // Or group in this window.
      for (const auto& group_pair : window.tab_groups) {
        const std::unique_ptr<sessions::tab_restore::Group>& group =
            group_pair.second;
        if (group->id == id || group->original_id == id) {
          return i;
        }
      }
    } else if ((*i)->type == tab_restore::Type::GROUP) {
      // Check if `id` matches a tab in this group.
      const auto& group = static_cast<const Group&>(**i);
      for (const auto& tab : group.tabs) {
        if (tab->id == id || tab->original_id == id) {
          return i;
        }
      }
    }
  }

  return entries_.end();
}

bool TabRestoreServiceHelper::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();

  if (entries_.empty()) {
    // Nothing to report
    return true;
  }

  std::string entries_dump_name =
      base::StringPrintf("tab_restore/service_helper_0x%" PRIXPTR "/entries",
                         reinterpret_cast<uintptr_t>(this));
  pmd->CreateAllocatorDump(entries_dump_name)
      ->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                  MemoryAllocatorDump::kUnitsObjects, entries_.size());

  for (const auto& entry : entries_) {
    const char* type_string = "";
    switch (entry->type) {
      case tab_restore::Type::WINDOW:
        type_string = "window";
        break;
      case tab_restore::Type::TAB:
        type_string = "tab";
        break;
      case tab_restore::Type::GROUP:
        type_string = "group";
        break;
    }

    std::string entry_dump_name = base::StringPrintf(
        "%s/%s_0x%" PRIXPTR, entries_dump_name.c_str(), type_string,
        reinterpret_cast<uintptr_t>(entry.get()));
    auto* entry_dump = pmd->CreateAllocatorDump(entry_dump_name);

    entry_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          entry->EstimateMemoryUsage());

    auto age = base::Time::Now() - entry->timestamp;
    entry_dump->AddScalar("age", MemoryAllocatorDump::kUnitsObjects,
                          age.InSeconds());

    if (system_allocator_name) {
      pmd->AddSuballocation(entry_dump->guid(), system_allocator_name);
    }
  }

  return true;
}

// static
bool TabRestoreServiceHelper::ValidateEntry(const Entry& entry) {
  switch (entry.type) {
    case tab_restore::Type::TAB:
      return ValidateTab(static_cast<const Tab&>(entry));
    case tab_restore::Type::WINDOW:
      return ValidateWindow(static_cast<const Window&>(entry));
    case tab_restore::Type::GROUP:
      return ValidateGroup(static_cast<const Group&>(entry));
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void TabRestoreServiceHelper::PopulateTab(Tab* tab,
                                          int index,
                                          LiveTabContext* context,
                                          LiveTab* live_tab) {
  tab->current_navigation_index = 0;
  if (!live_tab->IsInitialBlankNavigation() && live_tab->GetEntryCount() > 0) {
    AddSerializedNavigationEntries(
        live_tab, AddBehavior::kCurrentAndPreceedingEntries, tab->navigations);
    if (!tab->navigations.empty()) {
      tab->current_navigation_index =
          static_cast<int>(tab->navigations.size()) - 1;
    }
    AddSerializedNavigationEntries(
        live_tab, AddBehavior::kEntriesFollowingCurrentEntry, tab->navigations);
  }

  tab->timestamp = TimeNow();
  tab->tabstrip_index = index;
  tab->extension_app_id = client_->GetExtensionAppIDForTab(live_tab);
  tab->user_agent_override = live_tab->GetUserAgentOverride();
  tab->platform_data = live_tab->GetPlatformSpecificTabData();

  // Delegate may be NULL during unit tests.
  if (context) {
    tab->browser_id = context->GetSessionID().id();
    tab->pinned = context->IsTabPinned(tab->tabstrip_index);
    tab->group = context->GetTabGroupForTab(tab->tabstrip_index);

    if (tab->group.has_value()) {
      tab->saved_group_id =
          context->GetSavedTabGroupIdForGroup(tab->group.value());

      tab->group_visual_data =
          *context->GetVisualDataForGroup(tab->group.value());
    }

    tab->extra_data = context->GetExtraDataForTab(tab->tabstrip_index);
  }
}

LiveTabContext* TabRestoreServiceHelper::RestoreTab(
    const Tab& tab,
    LiveTabContext* context,
    WindowOpenDisposition disposition,
    sessions::tab_restore::Type session_restore_type,
    LiveTab** live_tab) {
  LiveTab* restored_tab;
  if (disposition == WindowOpenDisposition::CURRENT_TAB && context) {
    restored_tab = context->ReplaceRestoredTab(tab);
  } else {
    // We only respect the tab's original browser if there's no disposition.
    if (disposition == WindowOpenDisposition::UNKNOWN && tab.browser_id) {
      context = client_->FindLiveTabContextWithID(
          SessionID::FromSerializedValue(tab.browser_id));
    }

    // Restore a grouped tab into its original group, even if the group has
    // since been moved to a different context. If the original group doesn't
    // exist any more, fall back to using the tab's original browser.
    if (tab.group.has_value()) {
      // TODO: This needs to look at the saved id.
      LiveTabContext* group_context =
          client_->FindLiveTabContextWithGroup(tab.group.value());
      if (group_context) {
        context = group_context;
      }
    }

    int tab_index = -1;

    // |context| will be NULL in cases where one isn't already available (eg,
    // when invoked on Mac OS X with no windows open). In this case, create a
    // new browser into which we restore the tabs.
    if (context && disposition != WindowOpenDisposition::NEW_WINDOW) {
      tab_index = tab.tabstrip_index;
    } else {
      context = client_->CreateLiveTabContext(
          context, SessionWindow::TYPE_NORMAL, std::string(), gfx::Rect(),
          ui::mojom::WindowShowState::kNormal, std::string(), std::string(),
          std::map<std::string, std::string>());
      if (tab.browser_id) {
        UpdateTabBrowserIDs(tab.browser_id, context->GetSessionID());
      }
    }

    // Place the tab at the end if the tab index is no longer valid or
    // we were passed a specific disposition.
    if (tab_index < 0 || tab_index > context->GetTabCount() ||
        disposition != WindowOpenDisposition::UNKNOWN) {
      tab_index = context->GetTabCount();
    }

    restored_tab = context->AddRestoredTab(
        tab, tab_index,
        /*select=*/disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB,
        session_restore_type);
  }

  client_->OnTabRestored(
      tab.navigations.at(tab.current_navigation_index).virtual_url());
  if (live_tab) {
    *live_tab = restored_tab;
  }

  return context;
}

bool TabRestoreServiceHelper::ValidateTab(const Tab& tab) {
  return !tab.navigations.empty() &&
         static_cast<size_t>(tab.current_navigation_index) <
             tab.navigations.size();
}

bool TabRestoreServiceHelper::ValidateWindow(const Window& window) {
  if (static_cast<size_t>(window.selected_tab_index) >= window.tabs.size()) {
    return false;
  }

  for (const auto& tab : window.tabs) {
    if (!ValidateTab(*tab)) {
      return false;
    }
  }

  return true;
}

bool TabRestoreServiceHelper::ValidateGroup(const Group& group) {
  for (const auto& tab : group.tabs) {
    if (!ValidateTab(*tab)) {
      return false;
    }
  }

  return true;
}

bool TabRestoreServiceHelper::IsTabInteresting(const Tab& tab) {
  if (tab.navigations.empty()) {
    return false;
  }

  if (tab.navigations.size() > 1) {
    return true;
  }

  return tab.pinned ||
         tab.navigations.at(0).virtual_url() != client_->GetNewTabURL();
}

bool TabRestoreServiceHelper::IsWindowInteresting(const Window& window) {
  if (window.tabs.empty()) {
    return false;
  }

  if (window.tabs.size() > 1) {
    return true;
  }

  return IsTabInteresting(*window.tabs[0]);
}

bool TabRestoreServiceHelper::IsGroupInteresting(const Group& group) {
  return !group.tabs.empty();
}

bool TabRestoreServiceHelper::FilterEntry(const Entry& entry) {
  if (!ValidateEntry(entry)) {
    return false;
  }

  switch (entry.type) {
    case tab_restore::Type::TAB:
      return IsTabInteresting(static_cast<const Tab&>(entry));
    case tab_restore::Type::WINDOW:
      return IsWindowInteresting(static_cast<const Window&>(entry));
    case tab_restore::Type::GROUP:
      return IsGroupInteresting(static_cast<const Group&>(entry));
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void TabRestoreServiceHelper::UpdateTabBrowserIDs(SessionID::id_type old_id,
                                                  SessionID new_id) {
  for (const auto& entry : entries_) {
    if (entry->type == tab_restore::Type::TAB) {
      auto& tab = static_cast<Tab&>(*entry);
      if (tab.browser_id == old_id) {
        tab.browser_id = new_id.id();
      }
    } else if (entry->type == tab_restore::WINDOW) {
      auto& window = static_cast<Window&>(*entry);
      for (auto& tab : window.tabs) {
        tab->browser_id = new_id.id();
      }

      for (auto& group_pair : window.tab_groups) {
        Group& group = *group_pair.second.get();
        group.browser_id = new_id.id();
      }
    }
  }
}

base::Time TabRestoreServiceHelper::TimeNow() const {
  return time_factory_ ? time_factory_->TimeNow() : base::Time::Now();
}

}  // namespace sessions
