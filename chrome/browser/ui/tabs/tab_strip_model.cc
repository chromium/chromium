// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/commerce/browser_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/ui_utils.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

TabGroupModelFactory* factory_instance = nullptr;

// Works similarly to base::AutoReset but checks for access from the wrong
// thread as well as ensuring that the previous value of the re-entrancy guard
// variable was false.
class ReentrancyCheck {
 public:
  explicit ReentrancyCheck(bool* guard_flag) : guard_flag_(guard_flag) {
    CHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(!*guard_flag_);
    *guard_flag_ = true;
  }

  ~ReentrancyCheck() { *guard_flag_ = false; }

 private:
  const raw_ptr<bool> guard_flag_;
};

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occurred to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace

TabGroupModelFactory::TabGroupModelFactory() {
  DCHECK(!factory_instance);
  factory_instance = this;
}

// static
TabGroupModelFactory* TabGroupModelFactory::GetInstance() {
  if (!factory_instance) {
    factory_instance = new TabGroupModelFactory();
  }
  return factory_instance;
}

std::unique_ptr<TabGroupModel> TabGroupModelFactory::Create() {
  return std::make_unique<TabGroupModel>();
}

DetachedTabCollection::DetachedTabCollection(
    std::variant<std::unique_ptr<tabs::TabGroupTabCollection>,
                 std::unique_ptr<tabs::SplitTabCollection>> collection,
    std::optional<int> active_index,
    bool pinned)
    : collection_(std::move(collection)),
      active_index_(active_index),
      pinned_(pinned) {}

DetachedTabCollection::~DetachedTabCollection() = default;
DetachedTabCollection::DetachedTabCollection(DetachedTabCollection&&) = default;

DetachedTab::DetachedTab(int index_before_any_removals,
                         int index_at_time_of_removal,
                         bool was_pinned_at_time_of_removal,
                         std::unique_ptr<tabs::TabModel> tab,
                         TabStripModelChange::RemoveReason remove_reason,
                         tabs::TabInterface::DetachReason tab_detach_reason,
                         std::optional<SessionID> id)
    : tab(std::move(tab)),
      index_before_any_removals(index_before_any_removals),
      index_at_time_of_removal(index_at_time_of_removal),
      was_pinned_at_time_of_removal(was_pinned_at_time_of_removal),
      remove_reason(remove_reason),
      tab_detach_reason(tab_detach_reason),
      id(id) {}
DetachedTab::~DetachedTab() = default;
DetachedTab::DetachedTab(DetachedTab&&) = default;

// Holds all state necessary to send notifications for detached tabs.
struct TabStripModel::DetachNotifications {
  DetachNotifications(tabs::TabInterface* initially_active_tab,
                      const ui::ListSelectionModel& selection_model)
      : initially_active_tab(initially_active_tab),
        selection_model(selection_model) {}
  DetachNotifications(const DetachNotifications&) = delete;
  DetachNotifications& operator=(const DetachNotifications&) = delete;
  ~DetachNotifications() = default;

  // The tab that was active prior to any detaches happening. If this
  // is nullptr, the active tab was not removed.
  //
  // It's safe to use a raw pointer here because the active tab, if
  // detached, is owned by `detached_tab`.
  //
  // Once the notification for change of active tab has been sent,
  // this field is set to nullptr.
  raw_ptr<tabs::TabInterface> initially_active_tab = nullptr;

  // The WebContents that were recently detached. Observers need to be notified
  // about these. These must be updated after construction.
  std::vector<std::unique_ptr<DetachedTab>> detached_tab;

  // The selection model prior to any tabs being detached.
  const ui::ListSelectionModel selection_model;
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

constexpr int TabStripModel::kNoTab;

TabStripModel::TabStripModel(TabStripModelDelegate* delegate,
                             Profile* profile,
                             TabGroupModelFactory* group_model_factory)
    : delegate_(delegate),
      profile_(profile),
      selection_model_(std::make_unique<ui::ListSelectionModel>()),
      focused_group_(std::nullopt) {
  DCHECK(delegate_);

  contents_data_ = std::make_unique<tabs::TabStripCollection>(false);

  if (group_model_factory) {
    group_model_ = group_model_factory->Create();
  }
  scrubbing_metrics_.Init();
}

void TabStripModel::SetFocusedGroup(
    std::optional<tab_groups::TabGroupId> group) {
  CHECK(base::FeatureList::IsEnabled(features::kTabGroupsFocusing));

  if (focused_group_ == group) {
    return;
  }

  if (group.has_value() && group_model_ &&
      group_model_->ContainsTabGroup(group.value())) {
    const gfx::Range tabs_in_group =
        group_model_->GetTabGroup(group.value())->ListTabs();
    CHECK(!tabs_in_group.is_empty());

    // Copy the previous selection model, but remove tabs not part of the
    // tab_group in the list of selected tabs.
    ui::ListSelectionModel new_selection_model = selection_model();
    for (int index : selection_model_->selected_indices()) {
      if (index < static_cast<int>(tabs_in_group.start()) ||
          index >= static_cast<int>(tabs_in_group.end())) {
        new_selection_model.RemoveIndexFromSelection(index);
      }
    }

    // Update the anchor if its not within the tabgroup.
    if (new_selection_model.anchor() <
            static_cast<int>(tabs_in_group.start()) ||
        new_selection_model.anchor() >= static_cast<int>(tabs_in_group.end())) {
      new_selection_model.set_anchor(std::nullopt);
    }

    // Update the active tab if its not within the tabgroup.
    if (GetTabGroupForTab(new_selection_model.active().value()) != group) {
      new_selection_model.set_active(tabs_in_group.start());
      new_selection_model.AddIndexToSelection(tabs_in_group.start());
    }

    DCHECK(!new_selection_model.empty());
    SetSelection(std::move(new_selection_model),
                 TabStripModelObserver::CHANGE_REASON_NONE,
                 /*triggered_by_other_operation=*/false);
  }

  auto old_focused_group = focused_group_;
  focused_group_ = group;
  for (auto& observer : observers_) {
    observer.OnTabGroupFocusChanged(focused_group_, old_focused_group);
  }
}

TabStripModel::~TabStripModel() {
  for (auto& observer : observers_) {
    observer.ModelDestroyed(TabStripModelObserver::ModelPasskey(), this);
  }
}

void TabStripModel::SetTabStripUI(TabStripModelObserver* observer) {
  DCHECK(!tab_strip_ui_was_set_);

  std::vector<TabStripModelObserver*> new_observers{observer};
  for (auto& old_observer : observers_) {
    new_observers.push_back(&old_observer);
  }

  observers_.Clear();

  for (auto* new_observer : new_observers) {
    observers_.AddObserver(new_observer);
  }

  observer->StartedObserving(TabStripModelObserver::ModelPasskey(), this);
  tab_strip_ui_was_set_ = true;
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
  observer->StartedObserving(TabStripModelObserver::ModelPasskey(), this);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observer->StoppedObserving(TabStripModelObserver::ModelPasskey(), this);
  observers_.RemoveObserver(observer);
}

int TabStripModel::count() const {
  return contents_data_->TabCountRecursive();
}

bool TabStripModel::empty() const {
  return contents_data_->TabCountRecursive() == 0;
}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendWebContents(std::unique_ptr<WebContents> contents,
                                      bool foreground) {
  InsertWebContentsAt(
      count(), std::move(contents),
      foreground ? (ADD_INHERIT_OPENER | ADD_ACTIVE) : ADD_NONE);
}

void TabStripModel::AppendTab(std::unique_ptr<tabs::TabModel> tab,
                              bool foreground) {
  InsertDetachedTabAt(
      count(), std::move(tab),
      foreground ? (ADD_INHERIT_OPENER | ADD_ACTIVE) : ADD_NONE);
}

int TabStripModel::InsertWebContentsAt(
    int index,
    std::unique_ptr<WebContents> contents,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  return InsertDetachedTabAt(
      index, std::make_unique<tabs::TabModel>(std::move(contents), this),
      add_types, group);
}

int TabStripModel::InsertDetachedTabAt(
    int index,
    std::unique_ptr<tabs::TabModel> tab,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  tab->OnAddedToModel(this);
  return InsertTabAtImpl(index, std::move(tab), add_types, group);
}

std::unique_ptr<content::WebContents> TabStripModel::DiscardWebContentsAt(
    int index,
    std::unique_ptr<WebContents> new_contents) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  delegate()->WillAddWebContents(new_contents.get());

  CHECK(ContainsIndex(index));

  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveTab(), selection_model());
  WebContents* raw_new_contents = new_contents.get();
  std::unique_ptr<WebContents> old_contents =
      GetTabModelAtIndex(index)->DiscardContents(std::move(new_contents));

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    selection.new_contents = raw_new_contents;
    selection.reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  }

  TabStripModelChange::Replace replace;
  replace.tab = GetTabAtIndex(index);
  replace.old_contents = old_contents.get();
  replace.new_contents = raw_new_contents;
  replace.index = index;
  TabStripModelChange change(replace);
  OnChange(change, selection);

  return old_contents;
}

std::unique_ptr<tabs::TabModel> TabStripModel::DetachTabAtForInsertion(
    int index) {
  auto dt = DetachTabWithReasonAt(
      index, TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip,
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);
  return std::move(dt->tab);
}

std::unique_ptr<content::WebContents>
TabStripModel::DetachWebContentsAtForInsertion(
    int index,
    TabStripModelChange::RemoveReason reason) {
  auto dt = DetachTabWithReasonAt(index, reason,
                                  tabs::TabInterface::DetachReason::kDelete);
  return tabs::TabModel::DestroyAndTakeWebContents(std::move(dt->tab));
}

void TabStripModel::DetachAndDeleteWebContentsAt(int index) {
  // Drops the returned unique pointer.
  DetachTabWithReasonAt(index, TabStripModelChange::RemoveReason::kDeleted,
                        tabs::TabInterface::DetachReason::kDelete);
}

std::vector<std::variant<std::unique_ptr<DetachedTab>,
                         std::unique_ptr<DetachedTabCollection>>>
TabStripModel::DetachTabsAndCollectionsForInsertion(
    const std::vector<int>& tab_indices) {
  const std::vector<tab_groups::TabGroupId> groups_to_move =
      GetGroupsDestroyedFromRemovingIndices(tab_indices);

  std::vector<tabs::TabInterface*> tab_interfaces =
      GetTabsAtIndices(tab_indices);

  std::vector<std::variant<std::unique_ptr<DetachedTab>,
                           std::unique_ptr<DetachedTabCollection>>>
      owned_tabs_and_collections;

  for (const tabs::TabInterface* tab_interface : tab_interfaces) {
    const int index = GetIndexOfTab(tab_interface);
    if (index == TabStripModel::kNoTab) {
      // If this is a tab, we already moved it as part of its group.
      // If this is a header, we will move it when we get to its first tab.
      continue;
    }

    const std::optional<tab_groups::TabGroupId> group =
        tab_interface->GetGroup();
    if (std::find(groups_to_move.begin(), groups_to_move.end(), group) !=
        groups_to_move.end()) {
      owned_tabs_and_collections.emplace_back(
          DetachTabGroupForInsertion(group.value()));
    } else if (tab_interface->IsSplit()) {
      owned_tabs_and_collections.emplace_back(
          DetachSplitTabForInsertion(tab_interface->GetSplit().value()));
    } else {
      owned_tabs_and_collections.emplace_back(DetachTabWithReasonAt(
          index, TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip,
          tabs::TabInterface::DetachReason::kInsertIntoOtherWindow));
    }
  }

  return owned_tabs_and_collections;
}

std::unique_ptr<DetachedTab> TabStripModel::DetachTabWithReasonAt(
    int index,
    TabStripModelChange::RemoveReason web_contents_remove_reason,
    tabs::TabInterface::DetachReason tab_detach_reason) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(active_index(), kNoTab) << "Activate the TabStripModel by "
                                      "selecting at least one tab before "
                                      "trying to detach web contents.";
  tabs::TabModel* active_tab_model = GetTabModelAtIndex(active_index());
  tabs::TabModel* tab_model = GetTabModelAtIndex(index);
  if (index == active_index() && !closing_all_) {
    tab_model->WillDeactivate(base::PassKey<TabStripModel>());
  }
  if (tab_model->IsVisible()) {
    tab_model->WillBecomeHidden(base::PassKey<TabStripModel>());
  }
  tab_model->WillDetach(base::PassKey<TabStripModel>(), tab_detach_reason);

  DetachNotifications notifications(active_tab_model, selection_model());
  auto dt = DetachTabImpl(index, index,
                          /*create_historical_tab=*/false,
                          web_contents_remove_reason, tab_detach_reason);
  notifications.detached_tab.push_back(std::move(dt));
  SendDetachWebContentsNotifications(&notifications);
  return std::move(notifications.detached_tab[0]);
}

std::unique_ptr<DetachedTabCollection>
TabStripModel::DetachTabGroupForInsertion(
    const tab_groups::TabGroupId group_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(group_model_);
  CHECK(group_model_->ContainsTabGroup(group_id));

  std::map<split_tabs::SplitTabId,
           std::vector<std::pair<tabs::TabInterface*, int>>>
      splits_in_group;

  std::optional<int> active_index_in_collection = std::nullopt;
  int index = 0;
  for (tabs::TabInterface* tab :
       *contents_data_->GetTabGroupCollection(group_id)) {
    if (tab->IsActivated()) {
      active_index_in_collection = index;
    }
    if (tab->IsSplit()) {
      split_tabs::SplitTabId split_id = tab->GetSplit().value();
      if (!splits_in_group.contains(split_id)) {
        splits_in_group[split_id] = GetTabsAndIndicesInSplit(split_id);
      }
    }
    index++;
  }

  std::unique_ptr<tabs::TabCollection> detached_collection =
      DetachTabCollectionImpl(
          contents_data_->GetTabGroupCollection(group_id),
          base::BindOnce(&tabs::TabStripCollection::RemoveTabCollection,
                         base::Unretained(contents_data_.get()),
                         contents_data_->GetTabGroupCollection(group_id)),
          base::BindOnce(&TabStripModel::NotifyTabGroupDetached,
                         base::Unretained(this),
                         contents_data_->GetTabGroupCollection(group_id),
                         splits_in_group));

  if (focused_group_ == group_id) {
    SetFocusedGroup(std::nullopt);
  }

  return std::make_unique<DetachedTabCollection>(
      base::WrapUnique(static_cast<tabs::TabGroupTabCollection*>(
          detached_collection.release())),
      active_index_in_collection, false);
}

std::unique_ptr<DetachedTabCollection>
TabStripModel::DetachSplitTabForInsertion(
    const split_tabs::SplitTabId split_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(base::FeatureList::IsEnabled(features::kSideBySide));
  CHECK(GetSplitData(split_id));

  std::vector<std::pair<tabs::TabInterface*, int>> tabs_in_split =
      GetTabsAndIndicesInSplit(split_id);
  const bool previous_pinned_state = tabs_in_split[0].first->IsPinned();
  const std::optional<tab_groups::TabGroupId> previous_group_state =
      tabs_in_split[0].first->GetGroup();

  std::optional<int> active_index_in_collection = std::nullopt;
  int index = 0;
  for (tabs::TabInterface* tab :
       *contents_data_->GetSplitTabCollection(split_id)) {
    if (tab->IsActivated()) {
      active_index_in_collection = index;
      break;
    }
    index++;
  }

  std::unique_ptr<tabs::TabCollection> detached_collection =
      DetachTabCollectionImpl(
          contents_data_->GetSplitTabCollection(split_id),
          base::BindOnce(&tabs::TabStripCollection::RemoveTabCollection,
                         base::Unretained(contents_data_.get()),
                         contents_data_->GetSplitTabCollection(split_id)),
          base::BindOnce(&TabStripModel::NotifySplitTabDetached,
                         base::Unretained(this),
                         contents_data_->GetSplitTabCollection(split_id),
                         tabs_in_split, previous_group_state));

  return std::make_unique<DetachedTabCollection>(
      base::WrapUnique(static_cast<tabs::SplitTabCollection*>(
          detached_collection.release())),
      active_index_in_collection, previous_pinned_state);
}

gfx::Range TabStripModel::InsertDetachedSplitTabAt(
    std::unique_ptr<DetachedTabCollection> split,
    int index,
    bool pinned,
    std::optional<tab_groups::TabGroupId> group_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(std::holds_alternative<std::unique_ptr<tabs::SplitTabCollection>>(
      split->collection_));

  std::unique_ptr<tabs::SplitTabCollection> split_collection_unique_ptr =
      std::move(std::get<std::unique_ptr<tabs::SplitTabCollection>>(
          split->collection_));

  tabs::SplitTabCollection* split_collection =
      split_collection_unique_ptr.get();

  // Check a split with the same id is not present in the `contents_data_`.
  CHECK(!contents_data_->GetSplitTabCollection(
      split_collection->GetSplitTabId()));

  // Notify tab is added to model.
  for (tabs::TabInterface* tab : *split_collection) {
    static_cast<tabs::TabModel*>(tab)->OnAddedToModel(this);
  }

  return InsertDetachedCollectionImpl(
      split_collection, split->active_index_,
      base::BindOnce(&tabs::TabStripCollection::InsertTabCollectionAt,
                     base::Unretained(contents_data_.get()),
                     std::move(split_collection_unique_ptr), index, pinned,
                     group_id),
      base::BindOnce(&TabStripModel::NotifySplitTabAttached,
                     base::Unretained(this), split_collection));
}

gfx::Range TabStripModel::InsertDetachedTabGroupAt(
    std::unique_ptr<DetachedTabCollection> group,
    int index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(group_model_);
  CHECK(std::holds_alternative<std::unique_ptr<tabs::TabGroupTabCollection>>(
      group->collection_));

  std::unique_ptr<tabs::TabGroupTabCollection> group_collection_unique_ptr =
      std::move(std::get<std::unique_ptr<tabs::TabGroupTabCollection>>(
          group->collection_));

  tabs::TabGroupTabCollection* group_collection =
      group_collection_unique_ptr.get();

  CHECK(!group_model_->ContainsTabGroup(group_collection->GetTabGroupId()));

  // Notify tab is added to model.
  for (tabs::TabInterface* tab : *(group_collection)) {
    static_cast<tabs::TabModel*>(tab)->OnAddedToModel(this);
  }

  index = ConstrainInsertionIndex(index, false);

  return InsertDetachedCollectionImpl(
      group_collection, group->active_index_,
      base::BindOnce(&TabStripModel::InsertDetachedTabGroupImpl,
                     base::Unretained(this),
                     std::move(group_collection_unique_ptr), index),
      base::BindOnce(&TabStripModel::NotifyTabGroupAttached,
                     base::Unretained(this), group_collection));
}

tabs::TabModel* TabStripModel::GetTabModelAtIndex(int index) const {
  return static_cast<tabs::TabModel*>(GetTabAtIndex(index));
}

void TabStripModel::OnChange(const TabStripModelChange& change,
                             const TabStripSelectionChange& selection) {
  OnActiveTabChanged(selection);

  for (auto& observer : observers_) {
    observer.OnTabStripModelChanged(this, change, selection);
  }
}

TabStripModelChange::Remove TabStripModel::ProcessTabsForDetach(
    gfx::Range tab_indices) {
  TabStripModelChange::Remove remove;
  tabs::TabModel* active_tab_model = GetTabModelAtIndex(active_index());
  for (int index = tab_indices.end() - 1;
       index >= static_cast<int>(tab_indices.start()); index--) {
    tabs::TabModel* tab = GetTabModelAtIndex(index);

    // If the tab is active, notify it that it's going to be deactivated:
    if (tab == active_tab_model) {
      tab->WillDeactivate(base::PassKey<TabStripModel>());
    }
    // If the tab is visible, notify it that it's going to be hidden:
    if (tab->IsVisible()) {
      tab->WillBecomeHidden(base::PassKey<TabStripModel>());
    }

    // Tell the tab it’s being detached (inserted into another window).
    tab->WillDetach(base::PassKey<TabStripModel>(),
                    tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);

    // Notify observers that the tab will be removed.
    for (auto& observer : observers_) {
      observer.OnTabWillBeRemoved(tab->GetContents(), index);
    }

    // Fix opener relationships before removing the tab.
    FixOpeners(index);

    // Record this removal in the `Remove` event payload.
    remove.contents.emplace_back(
        tab, index,
        TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip,
        tabs::TabInterface::DetachReason::kInsertIntoOtherWindow, std::nullopt);
  }

  return remove;
}

void TabStripModel::UpdateSelectionModelForDetach(
    gfx::Range tab_indices,
    std::optional<int> next_selected_index) {
  const bool closed_all_tabs = (GetTabCount() == 0);
  bool active_tab_removed = tab_indices.Contains(gfx::Range(active_index()));

  if (closed_all_tabs) {
    selection_model_->Clear();
  } else {
    // Remove all the selected tabs from the model.
    for (int index = static_cast<int>(tab_indices.end()) - 1;
         index >= static_cast<int>(tab_indices.start()); --index) {
      selection_model_->DecrementFrom(index);
    }

    if (active_tab_removed) {
      if (!selection_model_->empty()) {
        selection_model_->set_active(
            *selection_model_->selected_indices().begin());
        selection_model_->set_anchor(selection_model_->active());
      } else {
        SetSelectedIndex(selection_model_.get(), next_selected_index.value());
      }
    }
  }
}

std::unique_ptr<tabs::TabCollection> TabStripModel::DetachTabCollectionImpl(
    tabs::TabCollection* collection,
    base::OnceCallback<std::unique_ptr<tabs::TabCollection>()>
        execute_detach_collection_operation,
    base::OnceClosure execute_tabs_notify_observer_operation) {
  //  Get Tabs and Indices in collection
  std::vector<int> tabs_in_collection;

  const int collection_start_index =
      GetIndexOfTab(collection->GetTabAtIndexRecursive(0));
  gfx::Range tab_indices =
      gfx::Range(collection_start_index,
                 collection_start_index + collection->TabCountRecursive());
  std::optional<int> next_selected_index =
      DetermineNewSelectedIndex(collection);

  tabs::TabModel* active_tab_model = GetTabModelAtIndex(active_index());
  const ui::ListSelectionModel old_selection_model = selection_model();
  bool selected_tabs_removed = std::any_of(
      selection_model_->selected_indices().begin(),
      selection_model_->selected_indices().end(),
      [&](int sel) { return tab_indices.Contains(gfx::Range(sel)); });

  // Pass the indices vector from above.
  TabStripModelChange::Remove remove = ProcessTabsForDetach(tab_indices);

  // Call the callback for detaching collection.
  std::unique_ptr<tabs::TabCollection> detached_collection =
      std::move(execute_detach_collection_operation).Run();

  // Pass the indices vector from above.
  UpdateSelectionModelForDetach(tab_indices, next_selected_index);

  ValidateTabStripModel();

  // Call the callback for collection detached.
  std::move(execute_tabs_notify_observer_operation).Run();

  // Notify tab is removed from model
  for (tabs::TabInterface* tab : *collection) {
    static_cast<tabs::TabModel*>(tab)->OnRemovedFromModel();
  }

  // TODO(crbug.com/418764233): Integrate with
  // SendDetachWebContentsNotifications
  TabStripModelChange change(std::move(remove));
  TabStripSelectionChange selection(active_tab_model, old_selection_model);
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();
  selection.new_model = selection_model();
  selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
  selection.selected_tabs_were_removed = selected_tabs_removed;

  OnChange(change, selection);

  if (empty()) {
    for (auto& observer : observers_) {
      observer.TabStripEmpty();
    }
  }

  return detached_collection;
}

gfx::Range TabStripModel::InsertDetachedCollectionImpl(
    tabs::TabCollection* collection,
    std::optional<int> active_index,
    base::OnceClosure execute_insert_detached_tabs_operation,
    base::OnceClosure execute_tabs_notify_observer_operation) {
  for (const tabs::TabInterface* tab : *collection) {
    delegate()->WillAddWebContents(tab->GetContents());
  }

  tabs::TabInterface* old_active_tab = GetActiveTab();
  const bool tab_strip_empty_initially = empty();

  // Add the collection.
  std::move(execute_insert_detached_tabs_operation).Run();

  int collection_insertion_index =
      GetIndexOfTab(collection->GetTabAtIndexRecursive(0));
  // Update selection model.
  for (int i = collection_insertion_index;
       i < collection_insertion_index +
               static_cast<int>(collection->TabCountRecursive());
       i++) {
    selection_model_->IncrementFrom(collection_insertion_index);
  }

  TabStripSelectionChange selection(old_active_tab, selection_model());
  if (active_index.has_value()) {
    SetSelectedIndex(selection_model_.get(),
                     collection_insertion_index + active_index.value());
  } else if (tab_strip_empty_initially) {
    SetSelectedIndex(selection_model_.get(), collection_insertion_index);
  }

  ValidateTabStripModel();

  for (tabs::TabInterface* tab : *collection) {
    static_cast<tabs::TabModel*>(tab)->DidInsert(
        base::PassKey<TabStripModel>());
  }

  // Send add notifications for tabs.
  selection.new_model = selection_model();
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();
  TabStripModelChange::Insert insert;

  for (int index_of_tab = GetIndexOfTab(*(collection->begin()));
       tabs::TabInterface* tab : *collection) {
    insert.contents.push_back({tab, tab->GetContents(), index_of_tab});
    index_of_tab++;
  }
  TabStripModelChange change(std::move(insert));
  OnChange(change, selection);

  // observer callback
  std::move(execute_tabs_notify_observer_operation).Run();

  return gfx::Range(
      collection_insertion_index,
      collection_insertion_index + collection->TabCountRecursive());
}

void TabStripModel::InsertDetachedTabGroupImpl(
    std::unique_ptr<tabs::TabGroupTabCollection> group_collection,
    int index) {
  group_model_->AddTabGroup(group_collection->GetTabGroup(),
                            base::PassKey<TabStripModel>());
  contents_data_->InsertTabCollectionAt(std::move(group_collection), index,
                                        false, std::nullopt);
}

std::unique_ptr<DetachedTab> TabStripModel::DetachTabImpl(
    int index_before_any_removals,
    int index_at_time_of_removal,
    bool create_historical_tab,
    TabStripModelChange::RemoveReason web_contents_remove_reason,
    tabs::TabInterface::DetachReason tab_detach_reason) {
  if (empty()) {
    return nullptr;
  }
  CHECK(ContainsIndex(index_at_time_of_removal));

  tabs::TabModel* tab = GetTabModelAtIndex(index_at_time_of_removal);

  const bool was_pinned_at_time_of_removal = tab->IsPinned();

  for (auto& observer : observers_) {
    observer.OnTabWillBeRemoved(tab->GetContents(), index_at_time_of_removal);
  }

  FixOpeners(index_at_time_of_removal);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database.

  std::optional<SessionID> id = std::nullopt;
  if (create_historical_tab) {
    id = delegate_->CreateHistoricalTab(tab->GetContents());
  }

  std::unique_ptr<tabs::TabModel> old_tab_model =
      RemoveTabFromIndexImpl(index_at_time_of_removal, tab_detach_reason);

  old_tab_model->OnRemovedFromModel();
  return std::make_unique<DetachedTab>(
      index_before_any_removals, index_at_time_of_removal,
      was_pinned_at_time_of_removal, std::move(old_tab_model),
      web_contents_remove_reason, tab_detach_reason, id);
}

void TabStripModel::SendDetachWebContentsNotifications(
    DetachNotifications* notifications) {
  // Sort the DetachedTab in decreasing order of
  // |index_before_any_removals|. This is because |index_before_any_removals| is
  // used by observers to update their own copy of TabStripModel state, and each
  // removal affects subsequent removals of higher index.
  std::sort(
      notifications->detached_tab.begin(), notifications->detached_tab.end(),
      [](const std::unique_ptr<DetachedTab>& dt1,
         const std::unique_ptr<DetachedTab>& dt2) {
        return dt1->index_before_any_removals > dt2->index_before_any_removals;
      });

  // `change` must be deleted before the unique_ptr<Tab>s in `notifications` are
  // reset, or their raw_ptr<Tab>s will dangle.
  {
    TabStripModelChange::Remove remove;
    for (auto& dt : notifications->detached_tab) {
      remove.contents.emplace_back(dt->tab.get(), dt->index_before_any_removals,
                                   dt->remove_reason, dt->tab_detach_reason,
                                   dt->id);
    }

    TabStripModelChange change(std::move(remove));

    TabStripSelectionChange selection;
    selection.old_tab = notifications->initially_active_tab;
    selection.new_tab = GetActiveTab();
    selection.old_contents =
        selection.old_tab ? selection.old_tab->GetContents() : nullptr;
    selection.new_contents = GetActiveWebContents();
    selection.old_model = notifications->selection_model;
    selection.new_model = selection_model();
    selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
    selection.selected_tabs_were_removed = std::ranges::any_of(
        notifications->detached_tab, [&notifications](auto& dt) {
          return notifications->selection_model.IsSelected(
              dt->index_before_any_removals);
        });
    OnChange(change, selection);

    // Prevent this from dangling in case a detached tab was initially active.
    notifications->initially_active_tab = nullptr;
  }

  for (auto& dt : notifications->detached_tab) {
    if (dt->remove_reason == TabStripModelChange::RemoveReason::kDeleted) {
      // This destroys the WebContents, which will also send
      // WebContentsDestroyed notifications.
      dt->tab.reset();
    }
  }

  if (empty()) {
    for (auto& observer : observers_) {
      observer.TabStripEmpty();
    }
  }
}

void TabStripModel::ActivateTabAt(int index,
                                  TabStripUserGestureDetails user_gesture) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));

  TRACE_EVENT0("ui", "TabStripModel::ActivateTabAt");

  scrubbing_metrics_.IncrementPressCount(user_gesture);

  ui::ListSelectionModel new_model(*selection_model_.get());
  SetSelectedIndex(&new_model, index);
  SetSelection(
      std::move(new_model),
      user_gesture.type != TabStripUserGestureDetails::GestureType::kNone
          ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
          : TabStripModelObserver::CHANGE_REASON_NONE,
      /*triggered_by_other_operation=*/false);
}

int TabStripModel::MoveWebContentsAt(int index,
                                     int to_position,
                                     bool select_after_move) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));
  const bool pinned = IsTabPinned(index);

  to_position = ConstrainMoveIndex(to_position, pinned);

  if (index == to_position) {
    return to_position;
  }

  std::optional<tab_groups::TabGroupId> group =
      GetGroupToAssign(index, to_position);
  MoveTabToIndexImpl(index, to_position, group, pinned, select_after_move);

  return to_position;
}

int TabStripModel::MoveWebContentsAt(
    int index,
    int to_position,
    bool select_after_move,
    std::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));

  bool pinned = IsTabPinned(index);
  to_position = ConstrainMoveIndex(to_position, pinned);
  MoveTabToIndexImpl(index, to_position, group, pinned, select_after_move);
  return to_position;
}

void TabStripModel::MoveSelectedTabsTo(
    int index,
    std::optional<tab_groups::TabGroupId> into_group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const std::vector<int> pinned_selected_indices = GetSelectedPinnedTabs();
  const std::vector<int> unpinned_selected_indices = GetSelectedUnpinnedTabs();

  const int last_pinned_index =
      std::clamp(index + static_cast<int>(pinned_selected_indices.size()) - 1,
                 static_cast<int>(pinned_selected_indices.size()) - 1,
                 pinned_tab_count - 1);

  MoveTabsToIndexImpl(
      pinned_selected_indices,
      last_pinned_index - static_cast<int>(pinned_selected_indices.size()) + 1,
      std::nullopt);

  const int first_unpinned_index =
      std::clamp(index + static_cast<int>(pinned_selected_indices.size()),
                 pinned_tab_count,
                 count() - static_cast<int>(unpinned_selected_indices.size()));

  MoveTabsToIndexImpl(unpinned_selected_indices, first_unpinned_index,
                      into_group);
}

void TabStripModel::MoveGroupTo(const tab_groups::TabGroupId& group,
                                int to_index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(to_index, kNoTab);
  to_index = ConstrainMoveIndex(to_index, false /* pinned tab */);

  if (!group_model_) {
    return;
  }

  const gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  if (static_cast<int>(tabs_in_group.start()) == to_index) {
    return;
  }

  std::optional<split_tabs::SplitTabId> destination_split =
      MoveBreaksSplitContiguity(static_cast<int>(tabs_in_group.start()),
                                tabs_in_group.length(), to_index);
  if (destination_split.has_value()) {
    RemoveSplitImpl(destination_split.value(),
                    SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
  }

  MoveGroupToImpl(group, to_index);
}

void TabStripModel::MoveSplitTo(
    const split_tabs::SplitTabId& split_id,
    int to_index,
    bool pinned,
    std::optional<tab_groups::TabGroupId> group_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  static const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>({tabs::TabCollection::Type::SPLIT});

  CHECK_NE(to_index, kNoTab);

  if (!group_model_ && group_id.has_value()) {
    return;
  }

  to_index = ConstrainMoveIndex(to_index, pinned);

  std::vector<std::pair<tabs::TabInterface*, int>> tabs_with_indices =
      GetTabsAndIndicesInSplit(split_id);

  // Invalid to move an to an index that breaks another split.
  std::optional<split_tabs::SplitTabId> destination_split =
      MoveBreaksSplitContiguity(tabs_with_indices[0].second,
                                tabs_with_indices.size(), to_index);
  CHECK(!destination_split.has_value());

  std::vector<int> tab_indices;
  for (const auto& tab_pair : tabs_with_indices) {
    tab_indices.push_back(tab_pair.second);
  }

  MoveTabsWithNotifications(
      tab_indices, to_index,
      base::BindOnce(&tabs::TabStripCollection::MoveTabsRecursive,
                     base::Unretained(contents_data_.get()), tab_indices,
                     to_index, group_id, pinned, kRetainCollectionTypes));
}

void TabStripModel::MoveGroupToImpl(const tab_groups::TabGroupId& group,
                                    int to_index) {
  const gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  CHECK_GT(tabs_in_group.length(), 0u);

  std::vector<int> tab_indices;
  for (size_t i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
    tab_indices.push_back(i);
  }

  static const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>(
          {tabs::TabCollection::Type::SPLIT, tabs::TabCollection::Type::GROUP});

  // Remove all the tabs from the model.
  MoveTabsWithNotifications(
      tab_indices, to_index,
      base::BindOnce(&tabs::TabStripCollection::MoveTabsRecursive,
                     base::Unretained(contents_data_.get()), tab_indices,
                     to_index, std::nullopt, false, kRetainCollectionTypes));

  NotifyTabGroupMoved(group);
}

WebContents* TabStripModel::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

tabs::TabInterface* TabStripModel::GetActiveTab() const {
  const int index = active_index();
  if (ContainsIndex(index)) {
    return GetTabAtIndex(index);
  }
  return nullptr;
}

std::vector<tabs::TabInterface*> TabStripModel::GetForegroundTabs() const {
  tabs::TabInterface* active_tab = GetActiveTab();
  if (!active_tab) {
    return std::vector<tabs::TabInterface*>();
  }
  if (active_tab->IsSplit()) {
    return GetSplitData(active_tab->GetSplit().value())->ListTabs();
  }
  return std::vector<tabs::TabInterface*>{active_tab};
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  if (ContainsIndex(index)) {
    return GetTabAtIndex(index)->GetContents();
  }
  return nullptr;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  int index = 0;
  for (const tabs::TabInterface* tab : *this) {
    if (tab->GetContents() == contents) {
      return index;
    }
    index++;
  }
  return kNoTab;
}

void TabStripModel::NotifyTabChanged(const tabs::TabInterface* const tab,
                                     TabChangeType change_type) {
  const int index = GetIndexOfTab(tab);
  for (auto& observer : observers_) {
    observer.TabChangedAt(tab->GetContents(), index, change_type);
  }
}

void TabStripModel::UpdateWebContentsStateAt(int index,
                                             TabChangeType change_type) {
  const tabs::TabInterface* const tab = GetTabAtIndex(index);

  for (auto& observer : observers_) {
    observer.TabChangedAt(tab->GetContents(), index, change_type);
  }
}

void TabStripModel::SetTabNeedsAttentionAt(int index, bool attention) {
  CHECK(ContainsIndex(index));

  for (auto& observer : observers_) {
    observer.SetTabNeedsAttentionAt(index, attention);
  }
}

void TabStripModel::SetTabGroupNeedsAttention(
    const tab_groups::TabGroupId& group,
    bool attention) {
  CHECK(group_model_->ContainsTabGroup(group));

  for (auto& observer : observers_) {
    observer.SetTabGroupNeedsAttention(group, attention);
  }
}

void TabStripModel::CloseAllTabs() {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(count());
  for (std::vector<tabs::TabInterface*> tabs =
           contents_data_->GetTabsRecursive();
       tabs::TabInterface* tab : base::Reversed(tabs)) {
    closing_tabs.push_back(tab->GetContents());
  }
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseAllTabsInGroup(const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  if (!group_model_) {
    return;
  }

  if (focused_group_ == group) {
    SetFocusedGroup(std::nullopt);
  }

  CloseAllTabsInGroupImpl(group);
}

void TabStripModel::CloseAllTabsInGroupImpl(
    const tab_groups::TabGroupId& group) {
  delegate_->WillCloseGroup(group);

  for (TabStripModelObserver& observer : observers_) {
    observer.OnTabGroupWillBeRemoved(group);
  }

  TabGroup* const tab_group = group_model_->GetTabGroup(group);
  tab_group->SetGroupIsClosing(/*is_closing=*/true);

  gfx::Range tabs_in_group = tab_group->ListTabs();
  if (static_cast<int>(tabs_in_group.length()) == count()) {
    closing_all_ = true;
  }

  std::vector<int> reversed_group_indices =
      gfx::Range(tabs_in_group.GetMax(), tabs_in_group.GetMin()).ToIntVector();
  std::vector<content::WebContents*> closing_tabs =
      GetWebContentsesByIndices(reversed_group_indices);
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  CHECK(ContainsIndex(index));
  CloseTabs({GetWebContentsAt(index)}, close_types);
}

bool TabStripModel::TabsNeedLoadingUI() const {
  for (const tabs::TabInterface* tab : *this) {
    if (tab->GetContents()->ShouldShowLoadingUI()) {
      return true;
    }
  }

  return false;
}

tabs::TabInterface* TabStripModel::GetOpenerOfTabAt(const int index) const {
  CHECK(ContainsIndex(index));
  const tabs::TabModel* const tab = GetTabModelAtIndex(index);

  return tab->opener();
}

void TabStripModel::SetOpenerOfWebContentsAt(int index, WebContents* opener) {
  CHECK(ContainsIndex(index));
  // The TabStripModel only maintains the references to openers that it itself
  // owns; trying to set an opener to an external WebContents can result in
  // the opener being used after its freed. See crbug.com/698681.
  DCHECK(!opener || GetIndexOfWebContents(opener) != kNoTab)
      << "Cannot set opener to a web contents not owned by this tab strip.";
  GetTabModelAtIndex(index)->set_opener(GetTabForWebContents(opener));
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  CHECK(ContainsIndex(start_index));

  std::set<const WebContents*> opener_and_descendants;
  opener_and_descendants.insert(opener);
  int last_index = kNoTab;

  for (int i = start_index + 1; i < count(); ++i) {
    tabs::TabModel* tab = GetTabModelAtIndex(i);
    // Test opened by transitively, i.e. include tabs opened by tabs opened by
    // opener, etc. Stop when we find the first non-descendant.
    if (!opener_and_descendants.count(
            tab->opener() ? tab->opener()->GetContents() : nullptr)) {
      // Skip over pinned tabs as new tabs are added after pinned tabs.
      if (tab->IsPinned()) {
        continue;
      }
      break;
    }
    opener_and_descendants.insert(tab->GetContents());
    last_index = i;
  }
  return last_index;
}

void TabStripModel::TabNavigating(WebContents* contents,
                                  ui::PageTransition transition) {
  if (ShouldForgetOpenersForTransition(transition)) {
    // Don't forget the openers if this tab is a New Tab page opened at the
    // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
    // navigation of one of these transition types before resetting the
    // opener relationships (this allows for the use case of opening a new
    // tab to do a quick look-up of something while viewing a tab earlier in
    // the strip). We can make this heuristic more permissive if need be.
    if (!IsNewTabAtEndOfTabStrip(contents)) {
      // If the user navigates the current tab to another page in any way
      // other than by clicking a link, we want to pro-actively forget all
      // TabStrip opener relationships since we assume they're beginning a
      // different task by reusing the current tab.
      ForgetAllOpeners();
    }
  }
}

void TabStripModel::SetTabBlocked(int index, bool blocked) {
  CHECK(ContainsIndex(index));
  tabs::TabModel* tab_model = GetTabModelAtIndex(index);
  if (tab_model->blocked() == blocked) {
    return;
  }
  tab_model->set_blocked(blocked);
  for (auto& observer : observers_) {
    observer.TabBlockedStateChanged(tab_model->GetContents(), index);
  }
}

int TabStripModel::SetTabPinned(int index, bool pinned) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(ContainsIndex(index));

  if (IsTabPinned(index) == pinned) {
    return index;
  }

  return SetTabPinnedImpl(index, pinned);
}

bool TabStripModel::IsTabPinned(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return index < IndexOfFirstNonPinnedTab();
}

bool TabStripModel::IsTabCollapsed(int index) const {
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  return group.has_value() && IsGroupCollapsed(group.value());
}

bool TabStripModel::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  DCHECK(group_model_);

  return group_model()->ContainsTabGroup(group) &&
         group_model()->GetTabGroup(group)->visual_data()->is_collapsed();
}

std::optional<split_tabs::SplitTabId> TabStripModel::GetSplitForTab(
    int index) const {
  CHECK(ContainsIndex(index));
  return GetTabAtIndex(index)->GetSplit();
}

bool TabStripModel::IsTabBlocked(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return GetTabModelAtIndex(index)->blocked();
}

bool TabStripModel::IsTabInForeground(int index) const {
  if (!ContainsIndex(index)) {
    return false;
  }

  const tabs::TabInterface *active_tab = GetActiveTab();
  if (!active_tab) {
    return false;
  }

  if (active_tab->IsSplit()) {
    const gfx::Range index_range =
        GetIndexRangeOfSplit(active_tab->GetSplit().value());

    // If the active tab is a split, check if the index is within the range of
    // the split since all of these tabs are in the foreground.
    return (index >= static_cast<int>(index_range.GetMin()) &&
            index < static_cast<int>(index_range.GetMax()));
  }

  return active_index() == index;
}

bool TabStripModel::IsTabClosable(int index) const {
  return PolicyAllowsTabClosing(GetWebContentsAt(index));
}

bool TabStripModel::IsTabClosable(const content::WebContents* contents) const {
  return IsTabClosable(GetIndexOfWebContents(contents));
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetTabGroupForTab(
    int index) const {
  return ContainsIndex(index) ? GetTabAtIndex(index)->GetGroup() : std::nullopt;
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetActiveTabGroupId()
    const {
  return GetTabGroupForTab(active_index());
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetSurroundingTabGroup(
    int index) const {
  if (!ContainsIndex(index - 1) || !ContainsIndex(index)) {
    return std::nullopt;
  }

  // If the tab before is not in a group, a tab inserted at |index|
  // wouldn't be surrounded by one group.
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index - 1);
  if (!group) {
    return std::nullopt;
  }

  // If the tab after is in a different (or no) group, a new tab at
  // |index| isn't surrounded.
  if (group != GetTabGroupForTab(index)) {
    return std::nullopt;
  }
  return group;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  return contents_data_->IndexOfFirstNonPinnedTab();
}

void TabStripModel::ExtendSelectionTo(int index) {
  CHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model(*selection_model_.get());
  if (!selection_model().anchor().has_value()) {
    SetSelectedIndex(&new_model, index);
  } else {
    new_model.SetSelectionFromAnchorTo(index);
    // Potentially expand the initial selection to capture any split tabs at the
    // anchor or index.
    std::pair<int, int> selection_range =
        GetSelectionRangeFromAnchorToIndex(index);
    new_model.AddIndexRangeToSelection(selection_range.first,
                                       selection_range.second);
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::SelectTabAt(int index) {
  if (!delegate()->IsTabStripEditable()) {
    return;
  }

  CHECK(ContainsIndex(index));

  const size_t selection_index = static_cast<size_t>(index);
  ui::ListSelectionModel new_model = selection_model();
  if (std::optional<split_tabs::SplitTabId> split_id = GetSplitForTab(index);
      split_id.has_value()) {
    gfx::Range index_range = GetIndexRangeOfSplit(split_id.value());
    new_model.AddIndexRangeToSelection(index_range.start(),
                                       index_range.end() - 1);
  } else {
    new_model.AddIndexToSelection(selection_index);
  }
  new_model.set_anchor(selection_index);
  new_model.set_active(selection_index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::DeselectTabAt(int index) {
  if (!delegate()->IsTabStripEditable()) {
    return;
  } else if (!IsTabSelected(index)) {
    // If the tab is already deselected, no need to do anything.
    return;
  } else if (selection_model().size() == 1 ||
             (selection_model().size() == 2 &&
              GetSplitForTab(index).has_value())) {
    // One tab must be selected and this tab is currently selected so we can't
    // unselect it.
    return;
  }

  CHECK(ContainsIndex(index));

  const size_t selection_index = static_cast<size_t>(index);
  ui::ListSelectionModel new_model = selection_model();
  if (std::optional<split_tabs::SplitTabId> split_id = GetSplitForTab(index);
      split_id.has_value()) {
    for (auto [_, i] : GetTabsAndIndicesInSplit(split_id.value())) {
      new_model.RemoveIndexFromSelection(i);
    }
  } else {
    new_model.RemoveIndexFromSelection(selection_index);
  }
  new_model.set_anchor(selection_index);
  if (!new_model.active().has_value() ||
      new_model.active() == selection_index) {
    new_model.set_active(*new_model.selected_indices().begin());
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model(*selection_model_.get());
  if (!selection_model().anchor().has_value()) {
    SetSelectedIndex(&new_model, index);
  } else {
    std::pair<int, int> selection_range =
        GetSelectionRangeFromAnchorToIndex(index);
    new_model.AddIndexRangeToSelection(selection_range.first,
                                       selection_range.second);
    new_model.set_active(index);
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::IsTabSelected(int index) const {
  CHECK(ContainsIndex(index));
  return selection_model().IsSelected(index);
}

void TabStripModel::SetSelectionFromModel(ui::ListSelectionModel source) {
  CHECK(source.active().has_value());
  const ui::ListSelectionModel::SelectedIndices sel = source.selected_indices();
  for (auto& source_sel_index : sel) {
    if (std::optional<split_tabs::SplitTabId> split_id =
            GetSplitForTab(source_sel_index);
        split_id.has_value()) {
      gfx::Range index_range = GetIndexRangeOfSplit(split_id.value());
      source.AddIndexRangeToSelection(index_range.start(),
                                      index_range.end() - 1);
    }
  }

  SetSelection(std::move(source), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

const ui::ListSelectionModel& TabStripModel::selection_model() const {
  return *selection_model_.get();
}

bool TabStripModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabStripModalUI> TabStripModel::ShowModalUI() {
  return std::make_unique<ScopedTabStripModalUIImpl>(this);
}

void TabStripModel::ForceShowingModalUIForTesting(bool showing) {
  showing_modal_ui_ = showing;
}

void TabStripModel::AddWebContents(
    std::unique_ptr<WebContents> contents,
    int index,
    ui::PageTransition transition,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  auto tab = std::make_unique<tabs::TabModel>(std::move(contents), this);
  AddTab(std::move(tab), index, transition, add_types, group);
}

void TabStripModel::AddTab(std::unique_ptr<tabs::TabModel> tab,
                           int index,
                           ui::PageTransition transition,
                           int add_types,
                           std::optional<tab_groups::TabGroupId> group) {
  for (auto& observer : observers_) {
    observer.OnTabWillBeAdded();
  }

  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // If the newly-opened tab is part of the same task as the parent tab, we want
  // to inherit the parent's opener attribute, so that if this tab is then
  // closed we'll jump back to the parent tab.
  bool inherit_opener = (add_types & ADD_INHERIT_OPENER) == ADD_INHERIT_OPENER;

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_LINK) &&
      (add_types & ADD_FORCE_INDEX) == 0) {
    // We assume tabs opened via link clicks are part of the same task as their
    // parent.  Note that when |force_index| is true (e.g. when the user
    // drag-and-drops a link to the tab strip), callers aren't really handling
    // link clicks, they just want to score the navigation like a link click in
    // the history backend, so we don't inherit the opener in this case.
    index = DetermineInsertionIndex(transition, add_types & ADD_ACTIVE);
    inherit_opener = true;

    // The current active index is our opener. If the tab we are adding is not
    // in a group, set the group of the tab to that of its opener.
    if (!group.has_value()) {
      group = GetTabGroupForTab(active_index());
    }
  } else {
    // For all other types, respect what was passed to us, normalizing -1s and
    // values that are too large.
    if (index < 0 || index > count()) {
      index = count();
    }
  }

  // Prevent the tab from being inserted at an index that would make the group
  // non-contiguous. Most commonly, the new-tab button always attempts to insert
  // at the end of the tab strip. Extensions can insert at an arbitrary index,
  // so we have to handle the general case.
  if (group_model_) {
    if (group.has_value()) {
      gfx::Range grouped_tabs =
          group_model_->GetTabGroup(group.value())->ListTabs();
      if (grouped_tabs.length() > 0) {
        index = std::clamp(index, static_cast<int>(grouped_tabs.start()),
                           static_cast<int>(grouped_tabs.end()));
      }
    } else if (GetTabGroupForTab(index - 1) == GetTabGroupForTab(index)) {
      group = GetTabGroupForTab(index);
    }

    // Pinned tabs cannot be added to a group.
    if (add_types & ADD_PINNED) {
      group = std::nullopt;
    }
  } else {
    group = std::nullopt;
  }

  // Move insertion index after the split group if it breaks contiguity.
  if (std::optional<split_tabs::SplitTabId> split_id =
          InsertionBreaksSplitContiguity(index);
      split_id.has_value()) {
    index = GetIndexRangeOfSplit(split_id.value()).GetMax();
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_TYPED) &&
      index == count()) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit opener as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-activated, not the next-adjacent.
    inherit_opener = true;
  }
  tabs::TabModel* tab_ptr = tab.get();
  tab->OnAddedToModel(this);
  InsertTabAtImpl(index, std::move(tab),
                  add_types | (inherit_opener ? ADD_INHERIT_OPENER : 0), group);
  // Reset the index, just in case insert ended up moving it on us.
  index = GetIndexOfTab(tab_ptr);

  // In the "quick look-up" case detailed above, we want to reset the opener
  // relationship on any active tab change, even to another tab in the same tree
  // of openers. A jump would be too confusing at that point.
  if (inherit_opener && ui::PageTransitionTypeIncludingQualifiersIs(
                            transition, ui::PAGE_TRANSITION_TYPED)) {
    tab_ptr->set_reset_opener_on_active_tab_change(true);
  }

  // TODO(sky): figure out why this is here and not in InsertWebContentsAt. When
  // here we seem to get failures in startup perf tests.
  // Ensure that the new WebContentsView begins at the same size as the
  // previous WebContentsView if it existed.  Otherwise, the initial WebKit
  // layout will be performed based on a width of 0 pixels, causing a
  // very long, narrow, inaccurate layout.  Because some scripts on pages (as
  // well as WebKit's anchor link location calculation) are run on the
  // initial layout and not recalculated later, we need to ensure the first
  // layout is performed with sane view dimensions even when we're opening a
  // new background tab.
  if (WebContents* old_contents = GetActiveWebContents()) {
    if ((add_types & ADD_ACTIVE) == 0) {
      tab_ptr->GetContents()->Resize(
          gfx::Rect(old_contents->GetContainerBounds().size()));
    }
  }
}

void TabStripModel::CloseSelectedTabs() {
  auto get_indices = base::BindRepeating(
      [](const ui::ListSelectionModel& selection_model) {
        const ui::ListSelectionModel::SelectedIndices& sel =
            selection_model.selected_indices();
        return std::vector<int>(sel.begin(), sel.end());
      },
      selection_model());

  ExecuteCloseTabsByIndicesCommand(std::move(get_indices),
                                   /*delete_groups=*/true);
}

void TabStripModel::SelectNextTab(TabStripUserGestureDetails detail) {
  SelectRelativeTab(TabRelativeDirection::kNext, detail);
}

void TabStripModel::SelectPreviousTab(TabStripUserGestureDetails detail) {
  SelectRelativeTab(TabRelativeDirection::kPrevious, detail);
}

void TabStripModel::SelectLastTab(TabStripUserGestureDetails detail) {
  ActivateTabAt(count() - 1, detail);
}

void TabStripModel::MoveTabNext() {
  MoveTabRelative(TabRelativeDirection::kNext);
}

void TabStripModel::MoveTabPrevious() {
  MoveTabRelative(TabRelativeDirection::kPrevious);
}

split_tabs::SplitTabData* TabStripModel::GetSplitData(
    split_tabs::SplitTabId split_id) const {
  const tabs::SplitTabCollection* split =
      contents_data_->GetSplitTabCollection(split_id);
  CHECK(split);
  return split->data();
}

std::set<split_tabs::SplitTabId> TabStripModel::ListSplits() const {
  return contents_data_->ListSplits();
}

bool TabStripModel::ContainsSplit(split_tabs::SplitTabId split_id) const {
  return contents_data_->GetSplitTabCollection(split_id);
}

bool TabStripModel::IsActiveTabSplit() const {
  const tabs::TabInterface* active_tab = GetActiveTab();
  return active_tab && active_tab->IsSplit();
}

std::optional<split_tabs::SplitTabId>
TabStripModel::InsertionBreaksSplitContiguity(int index) {
  CHECK(index >= 0 && index <= count());
  if (!ContainsIndex(index)) {
    return std::nullopt;
  }
  tabs::TabInterface* tab = GetTabAtIndex(index);
  if (tab->IsSplit() &&
      contents_data_->GetSplitTabCollection(tab->GetSplit().value())
              ->GetIndexOfTab(tab) > 0) {
    return tab->GetSplit();
  }
  return std::nullopt;
}

std::optional<split_tabs::SplitTabId> TabStripModel::MoveBreaksSplitContiguity(
    int start_index,
    int length,
    int final_index) {
  // The logic for finding the previous and next tabs depends on
  //  the relative position of the start_index and final_index as the indices of
  //  the previous tab and next tab get updated if start_index < final_index but
  //  otherwise the ordering is the same.
  const int previous_tab_index =
      start_index < final_index ? final_index - 1 + length : final_index - 1;

  const int next_tab_index = previous_tab_index + 1;

  if (!ContainsIndex(previous_tab_index) || !ContainsIndex(next_tab_index)) {
    return std::nullopt;
  }

  std::optional<split_tabs::SplitTabId> previous_split =
      GetSplitForTab(previous_tab_index);
  std::optional<split_tabs::SplitTabId> next_split =
      GetSplitForTab(next_tab_index);

  // If both previous and next splits are nullopt this will return nullopt.
  return (previous_split == next_split) ? previous_split : std::nullopt;
}

void TabStripModel::MaybeRemoveSplitsForMove(
    int initial_index,
    int final_index,
    const std::optional<tab_groups::TabGroupId> group,
    bool pin) {
  tabs::TabInterface* const tab = GetTabAtIndex(initial_index);
  const bool pinned_state_changed = tab->IsPinned() != pin;
  const bool group_state_changed = tab->GetGroup() != group;

  // This expects the tab should move in the collection hierarchy tree.
  CHECK((initial_index != final_index) || pinned_state_changed ||
        group_state_changed);

  // If the move is within a split collection there is no need to remove any
  // split.
  if (tab->IsSplit() &&
      tab->GetSplit() == GetTabAtIndex(final_index)->GetSplit() &&
      !pinned_state_changed && !group_state_changed) {
    return;
  }

  // Remove the split of the origin tab if it is not moving within the
  // split collection.
  if (tab->IsSplit()) {
    RemoveSplitImpl(tab->GetSplit().value(),
                    SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
  }

  // Maybe remove the split tab of the destination if it results in
  // discontiguity.
  std::optional<split_tabs::SplitTabId> destination_split =
      MoveBreaksSplitContiguity(initial_index, 1, final_index);

  if (destination_split.has_value()) {
    RemoveSplitImpl(destination_split.value(),
                    SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
  }
}

void TabStripModel::UpdateSplitLayout(split_tabs::SplitTabId split_id,
                                      split_tabs::SplitTabLayout tab_layout) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  split_tabs::SplitTabData* split_data = GetSplitData(split_id);

  if (split_data->visual_data()->split_layout() == tab_layout) {
    return;
  }

  split_tabs::SplitTabVisualData old_visual_data =
      *GetSplitData(split_id)->visual_data();

  split_data->visual_data()->set_split_layout(tab_layout);

  NotifySplitTabVisualsChanged(
      split_id, old_visual_data, *split_data->visual_data(),
      SplitTabChange::SplitVisualChangeReason::kLayoutUpdated);
}

void TabStripModel::UpdateSplitRatio(split_tabs::SplitTabId split_id,
                                     double split_ratio) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  split_tabs::SplitTabData* split_data = GetSplitData(split_id);
  if (split_data->visual_data()->split_ratio() == split_ratio) {
    return;
  }

  split_tabs::SplitTabVisualData old_visual_data = *split_data->visual_data();
  split_data->visual_data()->set_split_ratio(split_ratio);

  NotifySplitTabVisualsChanged(
      split_id, old_visual_data, *split_data->visual_data(),
      SplitTabChange::SplitVisualChangeReason::kRatioUpdated);
}

void TabStripModel::UpdateTabInSplit(tabs::TabInterface* split_tab,
                                     int update_index,
                                     SplitUpdateType update_type) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(base::FeatureList::IsEnabled(features::kSideBySide));

  tabs::TabInterface* update_tab = GetTabAtIndex(update_index);

  // Show the deletion dialog if the group is being deleted. In `kSwap` case the
  // group should be retained for the `update_index` since the active tab will
  // be swapped with it into the group.
  if (update_type == SplitUpdateType::kReplace &&
      update_tab->GetGroup().has_value() &&
      group_model_->GetTabGroup(update_tab->GetGroup().value())->tab_count() ==
          1) {
    std::vector<tab_groups::TabGroupId> groups_to_delete = {
        update_tab->GetGroup().value()};
    MarkTabGroupsForClosing(groups_to_delete);

    base::OnceCallback<void()> callback = base::BindOnce(
        &TabStripModel::UpdateTabInSplitImpl, base::Unretained(this), split_tab,
        update_index, update_type);

    return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                  std::move(callback));
  }

  UpdateTabInSplitImpl(split_tab, update_index, update_type);
}

void TabStripModel::ReverseTabsInSplit(split_tabs::SplitTabId split_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  tabs::TabInterface* first_tab = GetSplitData(split_id)->ListTabs()[0];
  const int index_of_first_tab_in_split = GetIndexOfTab(first_tab);
  MoveTabToIndexImpl(index_of_first_tab_in_split,
                     index_of_first_tab_in_split + 1, first_tab->GetGroup(),
                     first_tab->IsPinned(), false);
}

split_tabs::SplitTabId TabStripModel::AddToNewSplit(
    std::vector<int> indices,
    split_tabs::SplitTabVisualData visual_data,
    split_tabs::SplitTabCreatedSource source) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Ensure that there is only one index. This will be split with the active
  // tab.
  CHECK_EQ(indices.size(), 1u);
  CHECK(std::ranges::is_sorted(indices));
  CHECK(active_index() != kNoTab);
  CHECK(active_index() != indices[0]);

  split_tabs::RecordSplitTabCreated(source);

  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();

  // Insert the active index into the sorted `indices`.
  auto position = lower_bound(indices.begin(), indices.end(), active_index());
  indices.insert(position, active_index());

  AddToSplitImpl(split_id, indices, active_index(), visual_data,
                 SplitTabChange::SplitTabAddReason::kNewSplitTabAdded);
  split_tabs::LogSplitViewCreatedUKM(this, split_id);
  return split_id;
}

void TabStripModel::RestoreSplit(split_tabs::SplitTabId split_id,
                                 const std::vector<int>& indices,
                                 split_tabs::SplitTabVisualData visual_data) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(std::ranges::is_sorted(indices));
  CHECK_EQ(indices.size(), 2u);
  CHECK(features::IsRestoringSplitViewEnabled());

  // Ideally these are consecutive indices from the restore flow and the pivot
  // index does not matter. However, given there are numerous steps in restore
  // and split is the last step, the API should be resilient to potential
  // changes.
  AddToSplitImpl(split_id, indices, indices[0], visual_data,
                 SplitTabChange::SplitTabAddReason::kNewSplitTabAdded);
}

tab_groups::TabGroupId TabStripModel::AddToNewGroup(
    const std::vector<int> indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(SupportsTabGroups());

  // Ensure that the indices are nonempty, sorted, and unique.
  CHECK_GT(indices.size(), 0u);
  CHECK(std::ranges::is_sorted(indices));
  CHECK(std::ranges::adjacent_find(indices) == indices.end());

  // The odds of |new_group| colliding with an existing group are astronomically
  // low. If there is a collision, a DCHECK will fail in |AddToNewGroupImpl()|,
  // in which case there is probably something wrong with
  // |tab_groups::TabGroupId::GenerateNew()|.
  const tab_groups::TabGroupId new_group =
      tab_groups::TabGroupId::GenerateNew();
  AddToNewGroupImpl(indices, new_group);
  // TODO(crbug.com/339858272) : Consolidate all default save logic to
  // TabStripModel::AddToNewGroupImpl.
  delegate_->GroupAdded(new_group);

  for (TabStripModelObserver& observer : observers_) {
    observer.OnTabGroupAdded(new_group);
  }

  return new_group;
}

void TabStripModel::AddToExistingGroup(const std::vector<int> indices,
                                       const tab_groups::TabGroupId group,
                                       const bool add_to_end) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(SupportsTabGroups());

  // Ensure that the indices are sorted and unique.
  DCHECK(std::ranges::is_sorted(indices));
  DCHECK(std::ranges::adjacent_find(indices) == indices.end());
  CHECK(ContainsIndex(*(indices.begin())));
  CHECK(ContainsIndex(*(indices.rbegin())));

  AddToExistingGroupImpl(indices, group, add_to_end);
}

void TabStripModel::AddToGroupForRestore(const std::vector<int>& indices,
                                         const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  DCHECK(group_model_);
  if (!group_model_) {
    return;
  }

  const bool group_exists = group_model_->ContainsTabGroup(group);
  if (group_exists) {
    AddToExistingGroupImpl(indices, group);
  } else {
    AddToNewGroupImpl(indices, group);
  }
}

void TabStripModel::RemoveFromGroup(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_) {
    return;
  }

  std::map<tab_groups::TabGroupId, std::vector<int>> indices_per_tab_group;

  for (int index : indices) {
    std::optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
    if (old_group.has_value()) {
      indices_per_tab_group[old_group.value()].push_back(index);
    }
  }

  for (const auto& [immutable_group_id, immutable_group_indices] :
       indices_per_tab_group) {
    auto group_indices = immutable_group_indices;
    const TabGroup* group = group_model_->GetTabGroup(immutable_group_id);
    CHECK(group);
    tabs::TabInterface* first_tab_in_group = group->GetFirstTab();
    CHECK(first_tab_in_group);
    int first_tab_index = GetIndexOfTab(first_tab_in_group);

    tabs::TabInterface* last_tab_in_group = group->GetLastTab();
    int last_tab_index = GetIndexOfTab(last_tab_in_group);

    // TabGroupTabCollection::SeparateTabsByVisualPosition uses recursive
    // indices with respect to the group. Transpose the input by subtracting the
    // index of the first tab, and do the reverse on the output.
    std::transform(
        group_indices.begin(), group_indices.end(), group_indices.begin(),
        [first_tab_index](int index) { return index - first_tab_index; });
    auto [left_of_group, right_of_group] =
        contents_data_->GetTabGroupCollection(immutable_group_id)
            ->SeparateTabsByVisualPosition(group_indices);
    for (auto partition : {&left_of_group, &right_of_group}) {
      std::transform(
          partition->begin(), partition->end(), partition->begin(),
          [first_tab_index](int index) { return index + first_tab_index; });
    }

    MoveTabsAndSetPropertiesImpl(left_of_group, first_tab_index, std::nullopt,
                                 false);
    MoveTabsAndSetPropertiesImpl(right_of_group, last_tab_index + 1,
                                 std::nullopt, false);
  }
}

void TabStripModel::RemoveSplit(split_tabs::SplitTabId split_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  for (tabs::TabInterface* foreground_tab : GetForegroundTabs()) {
    if (!foreground_tab->IsActivated()) {
      static_cast<tabs::TabModel*>(foreground_tab)
          ->WillBecomeHidden(base::PassKey<TabStripModel>());
    }
  }

  RemoveSplitImpl(split_id,
                  SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
}

// Returns the ID of the group that is focused. If no group is focused,
// returns nullopt.
std::optional<tab_groups::TabGroupId> TabStripModel::GetFocusedGroup() const {
  if (!base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    return std::nullopt;
  }
  return focused_group_;
}

bool TabStripModel::IsReadLaterSupportedForAny(
    const std::vector<int>& indices) {
  if (!delegate_->SupportsReadLater()) {
    return false;
  }

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  if (!model || !model->loaded()) {
    return false;
  }
  for (int index : indices) {
    if (model->IsUrlSupported(
            chrome::GetURLToBookmark(GetWebContentsAt(index)))) {
      return true;
    }
  }
  return false;
}

void TabStripModel::AddToReadLater(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  AddToReadLaterImpl(indices);
}

void TabStripModel::OpenTabGroupEditor(const tab_groups::TabGroupId& group) {
  TabGroupChange change(this, group, TabGroupChange::kEditorOpened);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::ChangeTabGroupVisuals(
    const tab_groups::TabGroupId& group_id,
    tab_groups::TabGroupVisualData visual_data,
    bool is_customized) {
  TabGroup* tab_group = group_model_->GetTabGroup(group_id);

  // Move current visuals to old_visuals before updating
  tab_groups::TabGroupVisualData old_visuals = *tab_group->visual_data();
  TabGroupChange::VisualsChange visuals;
  visuals.old_visuals = &old_visuals;
  visuals.new_visuals = &visual_data;

  tab_group->SetVisualData(visual_data, is_customized);
  NotifyTabGroupVisualsChanged(group_id, visuals);
}

void TabStripModel::NotifyTabGroupVisualsChanged(
    const tab_groups::TabGroupId& group_id,
    TabGroupChange::VisualsChange visuals) {
  // Notify the controller of the visual change
  TabGroupChange change(this, group_id, visuals);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::NotifyTabGroupMoved(const tab_groups::TabGroupId& group) {
  TabGroupChange change(this, group, TabGroupChange::kMoved);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::NotifyTabGroupCreated(const tab_groups::TabGroupId& group) {
  TabGroupChange change(
      this, group,
      TabGroupChange::CreateChange(
          TabGroupChange::TabGroupCreationReason::kNewGroupCreated, nullptr));
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::NotifyTabGroupClosed(const tab_groups::TabGroupId& group) {
  TabGroupChange change(
      this, group,
      TabGroupChange::CloseChange(
          TabGroupChange::TabGroupClosureReason::kGroupClosed, nullptr));
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::NotifyTabGroupDetached(
    tabs::TabGroupTabCollection* group_collection,
    std::map<split_tabs::SplitTabId,
             std::vector<std::pair<tabs::TabInterface*, int>>>
        splits_in_group) {
  TabGroupChange change(
      this, group_collection->GetTabGroupId(),
      TabGroupChange::CloseChange(
          TabGroupChange::TabGroupClosureReason::kDetachedToAnotherTabstrip,
          group_collection));
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }

  group_model_->RemoveTabGroup(group_collection->GetTabGroupId(),
                               base::PassKey<TabStripModel>());

  for (auto const& [split_id, tabs_with_indices] : splits_in_group) {
    NotifySplitTabRemoved(
        split_id, tabs_with_indices,
        SplitTabChange::SplitTabRemoveReason::kDetachedToAnotherTabstrip);
  }
}

void TabStripModel::NotifyTabGroupAttached(
    tabs::TabGroupTabCollection* group_collection) {
  TabGroupChange change(
      this, group_collection->GetTabGroupId(),
      TabGroupChange::CreateChange(
          TabGroupChange::TabGroupCreationReason::kInsertedFromAnotherTabstrip,
          group_collection));
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }

  std::set<split_tabs::SplitTabId> splits_in_group;
  for (tabs::TabInterface* tab : *group_collection) {
    if (tab->IsSplit()) {
      splits_in_group.insert(tab->GetSplit().value());
    }
  }

  for (const split_tabs::SplitTabId& split_id : splits_in_group) {
    NotifySplitTabCreated(
        split_id, GetTabsAndIndicesInSplit(split_id),
        SplitTabChange::SplitTabAddReason::kInsertedFromAnotherTabstrip,
        *GetSplitData(split_id)->visual_data());
  }
}

void TabStripModel::NotifySplitTabCreated(
    split_tabs::SplitTabId split_id,
    const std::vector<std::pair<tabs::TabInterface*, int>>& tabs_with_indices,
    SplitTabChange::SplitTabAddReason reason,
    const split_tabs::SplitTabVisualData& visual_data) {
  SplitTabChange change(
      this, split_id,
      SplitTabChange::AddedChange(tabs_with_indices, reason, visual_data));

  for (auto& observer : observers_) {
    observer.OnSplitTabChanged(change);
  }
}

void TabStripModel::NotifySplitTabVisualsChanged(
    split_tabs::SplitTabId split_id,
    const split_tabs::SplitTabVisualData& old_visual_data,
    const split_tabs::SplitTabVisualData& new_visual_data,
    const SplitTabChange::SplitVisualChangeReason reason) {
  SplitTabChange change(
      this, split_id,
      SplitTabChange::VisualsChange(old_visual_data, new_visual_data, reason));

  for (auto& observer : observers_) {
    observer.OnSplitTabChanged(change);
  }
}

void TabStripModel::NotifySplitTabContentsUpdated(
    split_tabs::SplitTabId split_id,
    const std::vector<std::pair<tabs::TabInterface*, int>>& prev_tabs,
    const std::vector<std::pair<tabs::TabInterface*, int>>& new_tabs) {
  SplitTabChange change(this, split_id,
                        SplitTabChange::ContentsChange(prev_tabs, new_tabs));

  for (auto& observer : observers_) {
    observer.OnSplitTabChanged(change);
  }
}

void TabStripModel::NotifySplitTabRemoved(
    split_tabs::SplitTabId split_id,
    const std::vector<std::pair<tabs::TabInterface*, int>>& tabs_with_indices,
    SplitTabChange::SplitTabRemoveReason reason) {
  SplitTabChange change(
      this, split_id, SplitTabChange::RemovedChange(tabs_with_indices, reason));

  for (auto& observer : observers_) {
    observer.OnSplitTabChanged(change);
  }
}

void TabStripModel::NotifySplitTabDetached(
    tabs::SplitTabCollection* split_collection,
    std::vector<std::pair<tabs::TabInterface*, int>> tabs_in_split,
    std::optional<tab_groups::TabGroupId> previous_group_state) {
  // Send possible group notification of removal of grouped tabs.
  if (group_model_ && previous_group_state) {
    for (auto [tab, index] : tabs_in_split) {
      TabGroupStateChanged(index, tab, previous_group_state, std::nullopt);
    }
  }

  // Send split tab notification of removal.
  NotifySplitTabRemoved(
      split_collection->GetSplitTabId(), tabs_in_split,
      SplitTabChange::SplitTabRemoveReason::kDetachedToAnotherTabstrip);
}

void TabStripModel::NotifySplitTabAttached(
    tabs::SplitTabCollection* split_collection) {
  std::optional<tab_groups::TabGroupId> group_id =
      split_collection->GetTabAtIndexRecursive(0)->GetGroup();
  const split_tabs::SplitTabId& split_id = split_collection->GetSplitTabId();
  std::vector<std::pair<tabs::TabInterface*, int>> tabs_in_split =
      GetTabsAndIndicesInSplit(split_id);

  if (group_model_ && group_id.has_value()) {
    for (auto [tab, i] : tabs_in_split) {
      TabGroupStateChanged(i, tab, std::nullopt, group_id);
    }
  }

  // Send split attach notification
  NotifySplitTabCreated(
      split_id, tabs_in_split,
      SplitTabChange::SplitTabAddReason::kInsertedFromAnotherTabstrip,
      *GetSplitData(split_id)->visual_data());
}

int TabStripModel::GetTabCount() const {
  return contents_data_->TabCountRecursive();
}

TabStripModel::TabIterator TabStripModel::begin() const {
  return contents_data_->begin();
}
TabStripModel::TabIterator TabStripModel::end() const {
  return contents_data_->end();
}

const tabs::TabCollection* TabStripModel::Root(
    std::variant<base::PassKey<tabs_api::MojoTreeBuilder>,
                 base::PassKey<tabs_api::TabStripModelAdapterImpl>> key) const {
  return contents_data_.get();
}

const tabs::TabCollection* TabStripModel::GetRootForTesting() const {
  return contents_data_.get();
}

std::optional<const tab_groups::TabGroupId> TabStripModel::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle,
    base::PassKey<tabs_api::TabStripModelAdapterImpl>) const {
  return FindGroupIdFor(collection_handle);
}

tabs::TabCollectionHandle TabStripModel::GetPinnedTabsCollectionHandle(
    base::PassKey<tabs_api::TabStripModelAdapterImpl>) const {
  return contents_data_->pinned_collection()->GetHandle();
}

tabs::TabCollectionHandle TabStripModel::GetUnpinnedTabsCollectionHandle(
    base::PassKey<tabs_api::TabStripModelAdapterImpl>) const {
  return contents_data_->unpinned_collection()->GetHandle();
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  // Command must be valid.
  DCHECK(command_id > CommandFirst && command_id < CommandLast);

  // Context Index having an index greater than tab strip model doesnt make
  // sense since this context menu must target a tab.
  if (!ContainsIndex(context_index)) {
    return false;
  }

  switch (command_id) {
    case CommandNewTabToRight:
    case CommandCloseTab:
      return true;

    case CommandReload:
      return delegate_->CanReload();

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight: {
      return !GetIndicesClosedByCommand(context_index, command_id).empty();
    }
    case CommandDuplicate: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (int index : indices) {
        if (delegate()->CanDuplicateContentsAt(index)) {
          return true;
        }
      }
      return false;
    }

    case CommandToggleSiteMuted: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (int index : indices) {
        if (!GetWebContentsAt(index)->GetLastCommittedURL().is_empty()) {
          return true;
        }
      }
      return false;
    }

    case CommandTogglePinned:
      return true;

    case CommandToggleGrouped:
      return SupportsTabGroups();

    case CommandSendTabToSelf:
      return true;

    case CommandAddToReadLater:
      return true;

    case CommandAddToNewGroup:
      return SupportsTabGroups();

    case CommandAddToExistingGroup:
      return SupportsTabGroups();

    case CommandAddToSplit:
    case CommandSwapWithActiveSplit:
    case CommandArrangeSplit:
      return true;

    case CommandRemoveFromGroup:
      return SupportsTabGroups();

    case CommandMoveToExistingWindow:
      return true;

    case CommandMoveTabsToNewWindow: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      const bool would_leave_strip_empty =
          static_cast<int>(indices.size()) == count();
      return !would_leave_strip_empty &&
             delegate()->CanMoveTabsToWindow(indices);
    }

    case CommandOrganizeTabs:
      return true;

    case CommandCommerceProductSpecifications: {
      auto selected_web_contents =
          GetWebContentsesByIndices(GetIndicesForCommand(context_index));
      return commerce::IsProductSpecsMultiSelectMenuEnabled(
                 profile_, GetWebContentsAt(context_index)) &&
             commerce::IsWebContentsListEligibleForProductSpecs(
                 selected_web_contents);
    }

#if BUILDFLAG(ENABLE_GLIC)
    case CommandGlicShareLimit:
      return false;
    case CommandGlicStartShare:
      return true;
    case CommandGlicStopShare:
      return true;
#endif

    case CommandAddToNewComparisonTable:
    case CommandAddToExistingComparisonTable:
      return commerce::IsUrlEligibleForProductSpecs(
          GetWebContentsAt(context_index)->GetLastCommittedURL());

    case CommandCopyURL:
      DCHECK(delegate()->IsForWebApp());
      return true;

    case CommandGoBack:
      DCHECK(delegate()->IsForWebApp());
      return delegate()->CanGoBack(GetWebContentsAt(context_index));

    case CommandCloseAllTabs:
      DCHECK(delegate()->IsForWebApp());
      DCHECK(web_app::HasPinnedHomeTab(this));
      return true;

    default:
      NOTREACHED();
  }
}

void TabStripModel::ExecuteContextMenuCommand(int context_index,
                                              ContextMenuCommand command_id) {
  // This should have been tested by IsContextMenuCommandEnabled.
  CHECK(command_id > CommandFirst && command_id < CommandLast);

  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!ContainsIndex(context_index)) {
    return;
  }
  switch (command_id) {
    case CommandNewTabToRight: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.NewTabToRight.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", NewTabTypes::kNewTabContextMenu,
                                NewTabTypes::kNewTabEnumCount);
      delegate()->AddTabAt(GURL(), context_index + 1, true,
                           GetTabGroupForTab(context_index));
      break;
    }

    case CommandReload: {
      base::UmaHistogramCounts1000("Tab.ContextMenu.Reload.SelectedTabsCount",
                                   selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      if (!delegate()->CanReload()) {
        break;
      }
      const std::vector<int> indices = GetIndicesForCommand(context_index);
      base::UmaHistogramCounts100("TabStrip.Tab.ContextMenuReloadCount",
                                  indices.size());
      for (int index : indices) {
        WebContents* tab = GetWebContentsAt(index);
        if (tab) {
          tab->GetController().Reload(content::ReloadType::NORMAL, true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.Duplicate.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the tabs off as the indices will change as tabs are duplicated.
      std::vector<tabs::TabInterface*> tabs = GetTabsAtIndices(indices);

      for (size_t i = 0; i < tabs.size();) {
        tabs::TabInterface* tab = tabs[i];
        if (tab->IsSplit()) {
          split_tabs::SplitTabId split_id = tab->GetSplit().value();
          delegate()->DuplicateSplit(split_id);
          i += contents_data_->GetSplitTabCollection(split_id)
                   ->TabCountRecursive();
        } else {
          // Need to reacquire the index of the tab as that could have changed
          // since we got the tab from the index due to a previous tab being
          // duplicated.
          delegate()->DuplicateContentsAt(GetIndexOfTab(tab));
          i++;
        }
      }
      break;
    }

    case CommandCloseTab: {
      base::UmaHistogramCounts1000("Tab.ContextMenu.CloseTab.SelectedTabsCount",
                                   selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesForCommand,
                              base::Unretained(this), context_index),
          /*delete_groups=*/true);
      break;
    }

    case CommandCloseOtherTabs: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.CloseOtherTabs.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesClosedByCommand,
                              base::Unretained(this), context_index,
                              command_id),
          /*delete_groups=*/false);
      break;
    }

    case CommandCloseTabsToRight: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.CloseTabsToRight.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesClosedByCommand,
                              base::Unretained(this), context_index,
                              command_id),
          /*delete_groups=*/false);
      break;
    }

    case CommandSendTabToSelf: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.SendTabToSelf.SelectedTabsCount",
          selection_model().selected_indices().size());
      send_tab_to_self::ShowBubble(GetWebContentsAt(context_index));
      break;
    }

    case CommandTogglePinned: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.TogglePinned.SelectedTabsCount",
          selection_model().selected_indices().size());
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));

      std::vector<int> indices = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices);
      MarkTabGroupsForClosing(groups_to_delete);

      bool pin = WillContextMenuPin(context_index);

      // If there are groups that will be deleted by closing tabs from the
      // context menu, confirm the group deletion first, and then perform the
      // close, either through the callback provided to confirm, or directly if
      // the Confirm is allowing a synchronous delete.
      base::OnceCallback<void()> callback = base::BindOnce(
          [](TabStripModel* model, std::vector<int> indices, bool pin_indices) {
            model->SetTabsPinned(indices, pin_indices);
          },
          base::Unretained(this), indices, pin);

      if (pin && !groups_to_delete.empty()) {
        // If the delegate returns false for confirming the destroy of groups
        // that means that the user needs to make a decision about the
        // destruction first. prevent CloseTabs from being called.
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }

      break;
    }

    case CommandToggleGrouped: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.ToggleGrouped.SelectedTabsCount",
          selection_model().selected_indices().size());
      if (!group_model_) {
        break;
      }

      std::vector<int> indices = GetIndicesForCommand(context_index);
      if (WillContextMenuGroup(context_index)) {
        std::optional<tab_groups::TabGroupId> new_group_id =
            AddToNewGroup(indices);
        if (new_group_id.has_value()) {
          OpenTabGroupEditor(new_group_id.value());
        }
      } else {
        std::vector<tab_groups::TabGroupId> groups_to_delete =
            GetGroupsDestroyedFromRemovingIndices(indices);
        MarkTabGroupsForClosing(groups_to_delete);

        base::OnceCallback<void()> callback = base::BindOnce(
            &TabStripModel::RemoveFromGroup, base::Unretained(this), indices);
        if (!groups_to_delete.empty()) {
          delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                 std::move(callback));
        } else {
          std::move(callback).Run();
        }
      }
      break;
    }

    case CommandToggleSiteMuted: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.ToggleSiteMuted.SelectedTabsCount",
          selection_model().selected_indices().size());
      const bool mute = WillContextMenuMuteSites(context_index);
      if (mute) {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.MuteBy.TabStrip"));
      } else {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.UnmuteBy.TabStrip"));
      }
      SetSitesMuted(GetIndicesForCommand(context_index), mute);
      break;
    }

    case CommandAddToReadLater: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToReadLater.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(
          UserMetricsAction("DesktopReadingList.AddItem.FromTabContextMenu"));
      AddToReadLater(GetIndicesForCommand(context_index));
      break;
    }

    case CommandAddToNewGroup: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToNewGroup.SelectedTabsCount",
          selection_model().selected_indices().size());
      if (!group_model_) {
        break;
      }

      base::RecordAction(UserMetricsAction("TabContextMenu_AddToNewGroup"));
      AddToNewGroupFromContextIndex(context_index);
      break;
    }

    case CommandAddToExistingGroup: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToExistingGroup.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingGroupCommand with the correct group later.
      break;
    }

    case CommandAddToSplit: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToSplit.SelectedTabsCount",
          selection_model().selected_indices().size());
      CHECK(base::FeatureList::IsEnabled(features::kSideBySide));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // There are three cases for adding to a split.
      // 1. Selecting an inactive tab and making it a split with the active.
      // 2. Selecting active and inactive tab and creating a split
      // 3. Splitting the active tab with itself.
      // Remove the active tab from the indices first since splitting is done
      // with the active tab. Case 3 is a special zero split case that creates a
      // new split tab and is inferred by the delegate.
      std::erase_if(indices, [this](int tab_index) {
        return tab_index == active_index();
      });

      // This callback results in creating a split. It is either sent to the
      // deletion dialog that owns it and is responsible for calling it or if no
      // group is deleted it is simply called here.
      base::OnceCallback<void()> callback = base::BindOnce(
          &TabStripModelDelegate::NewSplitTab, base::Unretained(delegate_),
          indices, split_tabs::SplitTabCreatedSource::kTabContextMenu);

      // If we are splitting the active tab no group can be deleted.
      if (!indices.empty()) {
        std::vector<tab_groups::TabGroupId> groups_to_delete =
            GetGroupsDestroyedFromRemovingIndices(indices);
        if (!groups_to_delete.empty()) {
          MarkTabGroupsForClosing(groups_to_delete);
          return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                        std::move(callback));
        }
      }

      std::move(callback).Run();
      break;
    }

    case CommandSwapWithActiveSplit: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.SwapWithActiveSplit.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Do nothing. The submenu's delegate will invoke the correct subcommand
      // later.
      break;
    }

    case CommandArrangeSplit: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.ArrangeSplit.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Do nothing. The submenu's delegate will invoke the correct subcommand
      // later.
      break;
    }

    case CommandRemoveFromGroup: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.RemoveFromGroup.SelectedTabsCount",
          selection_model().selected_indices().size());
      if (!group_model_) {
        break;
      }

      base::RecordAction(UserMetricsAction("TabContextMenu_RemoveFromGroup"));

      std::vector<int> indices_to_remove = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices_to_remove);
      MarkTabGroupsForClosing(groups_to_delete);

      base::OnceCallback<void()> callback =
          base::BindOnce(&TabStripModel::RemoveFromGroup,
                         base::Unretained(this), indices_to_remove);
      if (!groups_to_delete.empty()) {
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }
      break;
    }

    case CommandMoveToExistingWindow: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.MoveToExistingWindow.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingWindowCommand with the correct window later.
      break;
    }

    case CommandMoveTabsToNewWindow: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.MoveTabsToNewWindow.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(
          UserMetricsAction("TabContextMenu_MoveTabToNewWindow"));

      std::vector<int> indices_to_move = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices_to_move);
      MarkTabGroupsForClosing(groups_to_delete);

      base::OnceCallback<void()> callback =
          base::BindOnce(&TabStripModelDelegate::MoveTabsToNewWindow,
                         base::Unretained(delegate()), indices_to_move);
      if (!groups_to_delete.empty()) {
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }
      break;
    }

    case CommandOrganizeTabs: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.OrganizeTabs.SelectedTabsCount",
          selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_OrganizeTabs"));
      const Browser* const browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(context_index));
      TabOrganizationService* const service =
          TabOrganizationServiceFactory::GetForProfile(profile_);
      CHECK(service);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.TabContextMenu.Clicked", true);

      service->RestartSessionAndShowUI(
          browser, TabOrganizationEntryPoint::kTabContextMenu,
          GetTabAtIndex(context_index));
      break;
    }

    case CommandCommerceProductSpecifications: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.CommerceProductSpecifications.SelectedTabsCount",
          selection_model().selected_indices().size());
      // ProductSpecs can only be triggered on non-incognito profiles.
      DCHECK(!profile_->IsIncognitoProfile());
      auto indices = GetIndicesForCommand(context_index);
      auto selected_web_contents =
          GetWebContentsesByIndices(GetIndicesForCommand(context_index));
      auto eligible_urls =
          commerce::GetListOfProductSpecsEligibleUrls(selected_web_contents);
      Browser* browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(context_index));
      chrome::OpenCommerceProductSpecificationsTab(browser, eligible_urls,
                                                   indices.back());
      break;
    }

#if BUILDFLAG(ENABLE_GLIC)
    case CommandGlicShareLimit:
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.GlicShareLimit.SelectedTabsCount",
          selection_model().selected_indices().size());
      break;
    case CommandGlicStopShare:
    case CommandGlicStartShare: {
      if (command_id == CommandGlicStartShare) {
        base::UmaHistogramCounts1000(
            "Tab.ContextMenu.GlicStartShare.SelectedTabsCount",
            selection_model().selected_indices().size());
      } else {
        base::UmaHistogramCounts1000(
            "Tab.ContextMenu.GlicStopShare.SelectedTabsCount",
            selection_model().selected_indices().size());
      }
      std::vector<int> indices = GetIndicesForCommand(context_index);
      std::vector<tabs::TabHandle> tab_handles;
      for (const auto& selection : indices) {
        tabs::TabInterface* tab = GetTabAtIndex(selection);
        if (command_id == CommandGlicStartShare &&
            delegate_->IsTabGlicPinned(tab->GetHandle())) {
          continue;
        }
        tab_handles.push_back(tab->GetHandle());
      }
      if (command_id == CommandGlicStartShare) {
        CHECK(delegate_->GlicPinTabs(tab_handles));
        if (!glic::GlicEnabling::IsMultiInstanceEnabled()) {
          delegate_->OpenGlicWindowFromSharedTab();
        }
      } else {
        CHECK(delegate_->GlicUnpinTabs(tab_handles));
      }
      break;
    }
#endif

    case CommandAddToNewComparisonTable: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToNewComparisonTable.SelectedTabsCount",
          selection_model().selected_indices().size());
      const auto& tab_url =
          GetWebContentsAt(context_index)->GetLastCommittedURL();
      commerce::OpenProductSpecsTabForUrls({tab_url}, this, context_index);

      break;
    }

    case CommandAddToExistingComparisonTable: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToExistingComparisonTable.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Handled by the existing comparison table submenu model.
      break;
    }

    case CommandCopyURL: {
      base::UmaHistogramCounts1000("Tab.ContextMenu.CopyURL.SelectedTabsCount",
                                   selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_CopyURL"));
      delegate()->CopyURL(GetWebContentsAt(context_index));
      break;
    }

    case CommandGoBack: {
      base::UmaHistogramCounts1000("Tab.ContextMenu.GoBack.SelectedTabsCount",
                                   selection_model().selected_indices().size());
      base::RecordAction(UserMetricsAction("TabContextMenu_Back"));
      delegate()->GoBack(GetWebContentsAt(context_index));
      break;
    }

    case CommandCloseAllTabs: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.CloseAllTabs.SelectedTabsCount",
          selection_model().selected_indices().size());
      // Closes all tabs except the pinned home tab.
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseAllTabs"));

      base::RepeatingCallback<std::vector<int>()> get_indices =
          base::BindRepeating(
              [](base::RepeatingCallback<int()> count) {
                std::vector<int> indices;
                for (int i = count.Run() - 1; i > 0; --i) {
                  indices.push_back(i);
                }
                return indices;
              },
              base::BindRepeating(&TabStripModel::count,
                                  base::Unretained(this)));

      // Because no tabs will remain in the tab strip after this command ensure
      // the groups are also deleted.
      ExecuteCloseTabsByIndicesCommand(get_indices,
                                       /*delete_groups=*/true);
      break;
    }
    case CommandAddToNewGroupFromMenuItem: {
      base::UmaHistogramCounts1000(
          "Tab.ContextMenu.AddToNewGroupFromMenuItem.SelectedTabsCount",
          selection_model().selected_indices().size());
      if (!group_model_) {
        break;
      }

      AddToNewGroupFromContextIndex(context_index);
      break;
    }
    case CommandFirst:
    case CommandAddNote:
    case CommandLast:
      NOTREACHED();
  }
}

void TabStripModel::AddToNewGroupFromContextIndex(int context_index) {
  std::vector<int> indices_to_add = GetIndicesForCommand(context_index);
  CHECK(!indices_to_add.empty());

  std::vector<tabs::TabInterface*> tabs_to_add =
      GetTabsAtIndices(indices_to_add);

  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(indices_to_add);
  MarkTabGroupsForClosing(groups_to_delete);

  base::OnceCallback<void()> callback = base::BindOnce(
      [](TabStripModel* model, std::vector<tabs::TabInterface*> tabs) {
        std::vector<int> indices;
        for (tabs::TabInterface* tab : tabs) {
          const int index = model->GetIndexOfTab(tab);
          if (index == kNoTab) {
            continue;
          }
          indices.push_back(index);
        }

        if (indices.empty()) {
          return;
        }

        std::optional<tab_groups::TabGroupId> new_group_id =
            model->AddToNewGroup(indices);
        model->OpenTabGroupEditor(new_group_id.value());
      },
      base::Unretained(this), tabs_to_add);

  if (groups_to_delete.empty()) {
    std::move(callback).Run();
  } else {
    delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                           std::move(callback));
  }
}

void TabStripModel::ExecuteAddToExistingGroupCommand(
    int context_index,
    const tab_groups::TabGroupId& group) {
  if (!group_model_ || !group_model_->ContainsTabGroup(group)) {
    return;
  }

  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingGroup"));

  if (!ContainsIndex(context_index)) {
    return;
  }

  std::vector<int> indices = GetIndicesForCommand(context_index);
  CHECK(!indices.empty());

  std::vector<tabs::TabInterface*> tabs = GetTabsAtIndices(indices);

  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(indices);
  MarkTabGroupsForClosing(groups_to_delete);

  // If there are no groups to delete OR there is only one group that was found
  // to be deleted, but it is the group that is being added to then the there
  // are no actual deletions occuring. Otherwise the group deletion must be
  // confirmed.
  base::OnceCallback<void()> callback = base::BindOnce(
      [](TabStripModel* model, std::vector<tabs::TabInterface*> tabs,
         const tab_groups::TabGroupId& group) {
        if (!model->group_model()->ContainsTabGroup(group)) {
          return;
        }
        std::vector<int> indices;
        for (tabs::TabInterface* tab : tabs) {
          const int index = model->GetIndexOfTab(tab);
          if (index == kNoTab) {
            continue;
          }
          indices.push_back(index);
        }

        model->AddToExistingGroup(indices, group, false);
      },
      base::Unretained(this), tabs, group);

  if (!groups_to_delete.empty() &&
      !(groups_to_delete.size() == 1 && groups_to_delete[0] == group)) {
    delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                           std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void TabStripModel::ExecuteAddToExistingWindowCommand(int context_index,
                                                      int browser_index) {
  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingWindow"));

  if (!ContainsIndex(context_index)) {
    return;
  }
  delegate()->MoveToExistingWindow(GetIndicesForCommand(context_index),
                                   browser_index);
}

std::vector<tab_groups::TabGroupId>
TabStripModel::GetGroupsDestroyedFromRemovingIndices(
    const std::vector<int>& indices) const {
  if (!SupportsTabGroups()) {
    return std::vector<tab_groups::TabGroupId>();
  }

  // Collect indices of tabs in each group.
  std::map<tab_groups::TabGroupId, std::vector<int>> group_indices_map;
  for (const int index : indices) {
    std::optional<tab_groups::TabGroupId> tab_group = GetTabGroupForTab(index);
    if (!tab_group.has_value()) {
      continue;
    }

    if (!group_indices_map.contains(tab_group.value())) {
      group_indices_map.emplace(tab_group.value(), std::vector<int>{});
    }

    group_indices_map[tab_group.value()].emplace_back(index);
  }

  // collect the groups that are going to be destoyed because all tabs are
  // closing.
  std::vector<tab_groups::TabGroupId> groups_to_delete;
  for (const auto& [group, group_indices] : group_indices_map) {
    if (group_model_->GetTabGroup(group)->tab_count() ==
        static_cast<int>(group_indices.size())) {
      groups_to_delete.emplace_back(group);
    }
  }
  return groups_to_delete;
}

void TabStripModel::ExecuteCloseTabsByIndices(
    base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
    uint32_t close_types) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  const std::vector<int> indices_to_close =
      std::move(get_indices_to_close).Run();
  CloseTabs(GetWebContentsesByIndices(indices_to_close), close_types);
}

void TabStripModel::MarkTabGroupsForClosing(
    const std::vector<tab_groups::TabGroupId> group_ids) {
  for (const tab_groups::TabGroupId& group_id : group_ids) {
    TabGroup* const tab_group = group_model()->GetTabGroup(group_id);
    CHECK(tab_group);
    tab_group->SetGroupIsClosing(true);
  }
}

void TabStripModel::ExecuteCloseTabsByIndicesCommand(
    base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
    bool delete_groups) {
  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(get_indices_to_close.Run());
  MarkTabGroupsForClosing(groups_to_delete);

  // If there are groups that will be deleted by closing tabs from the context
  // menu, confirm the group deletion first, and then perform the close, either
  // through the callback provided to confirm, or directly if the Confirm is
  // allowing a synchronous delete. The delegate gets to decide if the
  // groups will be deleted or closed based on where this is a bulk
  // operation.
  base::OnceCallback<void()> close_callback =
      base::BindOnce(&TabStripModel::ExecuteCloseTabsByIndices,
                     base::Unretained(this), get_indices_to_close,
                     TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                         TabCloseTypes::CLOSE_USER_GESTURE);

  if (!groups_to_delete.empty()) {
    // The delegate decides whether to close or delete the groups,
    // potentially prompting the user to decide what action to take.
    // ExecuteCloseTabs may or may not be called as a result.
    delegate_->OnGroupsDestruction(groups_to_delete, std::move(close_callback),
                                   delete_groups);
  } else {
    std::move(close_callback).Run();
  }
}

bool TabStripModel::WillContextMenuMuteSites(int index) {
  return !AreAllSitesMuted(*this, GetIndicesForCommand(index));
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i) {
    all_pinned = IsTabPinned(indices[i]);
  }
  return !all_pinned;
}

bool TabStripModel::WillContextMenuGroup(int index) {
  if (!group_model_) {
    return false;
  }

  std::vector<int> indices = GetIndicesForCommand(index);
  DCHECK(!indices.empty());

  // If all tabs are in the same group, then we ungroup, otherwise we group.
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(indices[0]);
  if (!group.has_value()) {
    return true;
  }

  for (size_t i = 1; i < indices.size(); ++i) {
    if (GetTabGroupForTab(indices[i]) != group) {
      return true;
    }
  }
  return false;
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  switch (cmd_id) {
    case CommandReload:
      *browser_cmd = IDC_RELOAD;
      break;
    case CommandDuplicate:
      *browser_cmd = IDC_DUPLICATE_TAB;
      break;
    case CommandSendTabToSelf:
      *browser_cmd = IDC_SEND_TAB_TO_SELF;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandOrganizeTabs:
      *browser_cmd = IDC_ORGANIZE_TABS;
      break;
    default:
      *browser_cmd = 0;
      return false;
  }

  return true;
}

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(
    const gfx::Range& block_tab_range) const {
  CHECK(ContainsIndex(block_tab_range.start()));
  CHECK(ContainsIndex(block_tab_range.end() - 1));

  std::set<tabs::TabInterface*> block_tabs;
  for (size_t i = block_tab_range.start(); i < block_tab_range.end(); i++) {
    block_tabs.insert(GetTabModelAtIndex(i));
  }

  for (size_t i = block_tab_range.end(); i < static_cast<size_t>(count());
       i++) {
    if (block_tabs.find(GetTabModelAtIndex(i)->opener()) != block_tabs.end()) {
      return i;
    }
  }

  for (int i = block_tab_range.start() - 1; i >= 0; i--) {
    if (block_tabs.find(GetTabModelAtIndex(i)->opener()) != block_tabs.end()) {
      return i;
    }
  }

  return kNoTab;
}

int TabStripModel::GetIndexOfNextWebContentsOpenedByOpenerOf(
    const gfx::Range& block_tab_range) const {
  CHECK(ContainsIndex(block_tab_range.start()));
  CHECK(ContainsIndex(block_tab_range.end() - 1));

  std::set<tabs::TabInterface*> block_openers;

  for (size_t i = block_tab_range.start(); i < block_tab_range.end(); ++i) {
    tabs::TabModel* tab = GetTabModelAtIndex(i);
    if (tab->opener()) {
      block_openers.insert(tab->opener());
    }
  }

  if (block_openers.empty()) {
    return kNoTab;
  }

  for (size_t i = block_tab_range.end(); i < static_cast<size_t>(count());
       i++) {
    if (block_openers.find(GetTabModelAtIndex(i)->opener()) !=
        block_openers.end()) {
      return i;
    }
  }

  for (int i = block_tab_range.start() - 1; i >= 0; i--) {
    if (block_openers.find(GetTabModelAtIndex(i)->opener()) !=
        block_openers.end()) {
      return i;
    }
  }

  return kNoTab;
}

std::optional<int> TabStripModel::GetNextExpandedActiveTab(
    const gfx::Range& block_tab_range) const {
  // Check tabs from the end of the block.
  for (int i = block_tab_range.end(); i < count(); ++i) {
    std::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()))) {
      return i;
    }
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = block_tab_range.start() - 1; i >= 0; --i) {
    std::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()))) {
      return i;
    }
  }

  return std::nullopt;
}

std::optional<int> TabStripModel::GetNextExpandedActiveTab(
    tab_groups::TabGroupId collapsing_group) const {
  CHECK(group_model()->ContainsTabGroup(collapsing_group));
  gfx::Range group_tab_indices =
      group_model()->GetTabGroup(collapsing_group)->ListTabs();
  return GetNextExpandedActiveTab(group_tab_indices);
}

void TabStripModel::ForgetAllOpeners() {
  for (tabs::TabInterface* tab : *this) {
    static_cast<tabs::TabModel*>(tab)->set_opener(nullptr);
  }
}

void TabStripModel::ForgetOpener(WebContents* contents) {
  const int index = GetIndexOfWebContents(contents);
  CHECK(ContainsIndex(index));
  GetTabModelAtIndex(index)->set_opener(nullptr);
}

void TabStripModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("active_index", active_index());
  dict.Add("tab_count", count());
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

bool TabStripModel::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return delegate_->RunUnloadListenerBeforeClosing(contents);
}

bool TabStripModel::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return contents->NeedToFireBeforeUnloadOrUnloadEvents() ||
         delegate_->ShouldRunUnloadListenerBeforeClosing(contents);
}

int TabStripModel::ConstrainInsertionIndex(int index, bool pinned_tab) const {
  return pinned_tab ? std::clamp(index, 0, IndexOfFirstNonPinnedTab())
                    : std::clamp(index, IndexOfFirstNonPinnedTab(), count());
}

int TabStripModel::ConstrainMoveIndex(int index, bool pinned_tab) const {
  return pinned_tab
             ? std::clamp(index, 0, IndexOfFirstNonPinnedTab() - 1)
             : std::clamp(index, IndexOfFirstNonPinnedTab(), count() - 1);
}

std::vector<int> TabStripModel::GetIndicesForCommand(int index) const {
  if (!IsTabSelected(index)) {
    // When the context menu is triggered on an unselected tab that is part of
    // the split, return all the tabs in that split so that context menu
    // actions can operate on the entire split.
    std::optional<split_tabs::SplitTabId> split = GetSplitForTab(index);
    if (split.has_value()) {
      gfx::Range index_range = GetIndexRangeOfSplit(split.value());
      std::vector<int> split_indices(index_range.length());
      std::iota(split_indices.begin(), split_indices.end(),
                static_cast<int>(index_range.start()));
      return split_indices;
    }
    return {index};
  }
  const ui::ListSelectionModel::SelectedIndices& sel =
      selection_model().selected_indices();
  return std::vector<int>(sel.begin(), sel.end());
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  std::vector<int> indices;
  if (!ContainsIndex(index)) {
    return indices;
  }
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int last_unclosed_tab = -1;
  if (id == CommandCloseTabsToRight) {
    last_unclosed_tab =
        is_selected ? *selection_model().selected_indices().rbegin() : index;
  }

  // If the tab that the context menu command is invoked on is not selected and
  // also in a split, also exclude tabs from that split from being closed. We
  // don't have to worry about the case when a split is selected, because all
  // indices in that split are guaranteed to be part of the selection model.
  tabs::TabInterface* invoked_tab = GetTabAtIndex(index);
  gfx::Range indices_to_exclude =
      invoked_tab->IsSplit()
          ? GetIndexRangeOfSplit(invoked_tab->GetSplit().value())
          : gfx::Range(index, index + 1);

  // NOTE: callers expect the vector to be sorted in descending order.
  for (int i = count() - 1; i > last_unclosed_tab; --i) {
    if (!indices_to_exclude.Contains(gfx::Range(i, i + 1)) && !IsTabPinned(i) &&
        (!is_selected || !IsTabSelected(i))) {
      indices.push_back(i);
    }
  }
  return indices;
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetLastCommittedURL();
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUINewTabHost &&
         contents == GetTabAtIndex(count() - 1)->GetContents() &&
         contents->GetController().GetEntryCount() == 1;
}

std::vector<content::WebContents*> TabStripModel::GetWebContentsesByIndices(
    std::vector<int> indices) const {
  bool reversed = false;
  if (std::is_sorted(indices.begin(), indices.end(),
                     [](int a, int b) { return a > b; })) {
    std::reverse(indices.begin(), indices.end());
    reversed = true;
  } else {
    CHECK(std::is_sorted(indices.begin(), indices.end()));
  }

  std::vector<tabs::TabInterface*> tabs = GetTabsAtIndices(indices);
  if (reversed) {
    std::reverse(tabs.begin(), tabs.end());
  }

  std::vector<content::WebContents*> result;
  result.reserve(tabs.size());
  for (tabs::TabInterface* tab : tabs) {
    result.push_back(tab->GetContents());
  }
  return result;
}

int TabStripModel::InsertTabAtImpl(
    int index,
    std::unique_ptr<tabs::TabModel> tab,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  if (group_model_ && group.has_value()) {
    CHECK(group_model_->ContainsTabGroup(group.value()));
  }

  delegate()->WillAddWebContents(tab->GetContents());

  const bool active = (add_types & ADD_ACTIVE) != 0 || empty();
  const bool pin = (add_types & ADD_PINNED) != 0;
  index = ConstrainInsertionIndex(index, pin);

  tabs::TabModel* const active_tab_model =
      selection_model().active().has_value()
          ? GetTabModelAtIndex(active_index())
          : nullptr;

  // If there's already an active tab, and the new tab will become active, send
  // a notification.
  if (active_tab_model && active && !closing_all_) {
    NotifyForegroundTabsWillEnterBackground();
  }

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  CHECK_EQ(this, tab->owning_model());
  if ((add_types & ADD_INHERIT_OPENER) && active_tab_model) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple openers active at the same time.
      ForgetAllOpeners();
    }
    tab->set_opener(active_tab_model);
  }

  // TODO(gbillock): Ask the modal dialog manager whether the WebContents should
  // be blocked, or just let the modal dialog manager make the blocking call
  // directly and not use this at all.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          tab->GetContents());
  if (manager) {
    tab->set_blocked(manager->IsDialogActive());
  }

  InsertTabAtIndexImpl(std::move(tab), index, group, pin, active);

  return index;
}

int TabStripModel::GetIndexOfTab(const tabs::TabInterface* tab) const {
  if (tab == nullptr) {
    return kNoTab;
  }

  std::optional<size_t> index_of_tab =
      contents_data_->GetIndexOfTabRecursive(tab);
  return index_of_tab.value_or(kNoTab);
}

tabs::TabInterface* TabStripModel::GetTabAtIndex(int index) const {
  return contents_data_->GetTabAtIndexRecursive(index);
}

std::vector<tabs::TabInterface*> TabStripModel::GetTabsAtIndices(
    const std::vector<int>& indices) const {
  std::vector<tabs::TabInterface*> result;
  result.reserve(indices.size());

  size_t indices_index = 0;
  int tab_index = 0;
  for (tabs::TabInterface* tab : *this) {
    if (indices_index == indices.size()) {
      break;
    }
    if (tab_index == indices[indices_index]) {
      result.push_back(tab);
      ++indices_index;
    }
    ++tab_index;
  }

  CHECK(indices_index == indices.size());
  return result;
}

tabs::TabInterface* TabStripModel::GetTabForWebContents(
    const content::WebContents* contents) const {
  return GetTabAtIndex(GetIndexOfWebContents(contents));
}

void TabStripModel::CloseTabs(base::span<content::WebContents* const> items,
                              uint32_t close_types) {
  std::vector<content::WebContents*> filtered_items;
  for (content::WebContents* contents : items) {
    if (IsTabClosable(contents)) {
      filtered_items.push_back(contents);
    } else {
      for (auto& observer : observers_) {
        observer.TabCloseCancelled(contents);
      }
    }
  }

  if (filtered_items.empty()) {
    return;
  }

  const bool closing_all = static_cast<int>(filtered_items.size()) == count();
  base::WeakPtr<TabStripModel> ref = weak_factory_.GetWeakPtr();
  if (closing_all) {
    for (auto& observer : observers_) {
      observer.WillCloseAllTabs(this);
    }
  }

  DetachNotifications notifications(GetActiveTab(), selection_model());
  const bool closed_all =
      CloseWebContentses(filtered_items, close_types, &notifications);

  // When unload handler is triggered for all items, we should wait for the
  // result.
  if (!notifications.detached_tab.empty()) {
    SendDetachWebContentsNotifications(&notifications);
  }

  if (!ref) {
    return;
  }
  if (closing_all) {
    // CloseAllTabsStopped is sent with reason kCloseAllCompleted if
    // closed_all; otherwise kCloseAllCanceled is sent.
    for (auto& observer : observers_) {
      observer.CloseAllTabsStopped(
          this, closed_all ? TabStripModelObserver::kCloseAllCompleted
                           : TabStripModelObserver::kCloseAllCanceled);
    }
  }
}

bool TabStripModel::CloseWebContentses(
    base::span<content::WebContents* const> items,
    uint32_t close_types,
    DetachNotifications* notifications) {
  if (items.empty()) {
    return true;
  }

  for (WebContents* contents : items) {
    const int index = GetIndexOfWebContents(contents);
    tabs::TabModel* tab_model = GetTabModelAtIndex(index);
    if (index == active_index() && !closing_all_) {
      tab_model->WillDeactivate(base::PassKey<TabStripModel>());
    }
    if (tab_model->IsVisible() && !closing_all_) {
      tab_model->WillBecomeHidden(base::PassKey<TabStripModel>());
    }
    tab_model->WillDetach(base::PassKey<TabStripModel>(),
                          tabs::TabInterface::DetachReason::kDelete);
  }

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (!browser_shutdown::HasShutdownStarted()) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* contents : items) {
      if (ShouldRunUnloadListenerBeforeClosing(contents)) {
        continue;
      }
      content::RenderProcessHost* process =
          contents->GetPrimaryMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes) {
      pair.first->FastShutdownIfPossible(pair.second, false);
    }
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool closed_all = true;

  // The indices of WebContents prior to any modification of the internal state.
  std::vector<int> original_indices;
  original_indices.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    original_indices[i] = GetIndexOfWebContents(items[i]);
  }

  std::vector<std::unique_ptr<DetachedTab>> detached_tab;
  for (size_t i = 0; i < items.size(); ++i) {
    WebContents* closing_contents = items[i];

    // The index into contents_data_.
    int current_index = GetIndexOfWebContents(closing_contents);
    CHECK_NE(current_index, kNoTab);

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & TabCloseTypes::CLOSE_USER_GESTURE);
    }

    if (RunUnloadListenerBeforeClosing(closing_contents)) {
      closed_all = false;
      continue;
    }

    bool create_historical_tab =
        close_types & TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB;
    auto dt =
        DetachTabImpl(original_indices[i], current_index, create_historical_tab,
                      TabStripModelChange::RemoveReason::kDeleted,
                      tabs::TabInterface::DetachReason::kDelete);
    detached_tab.push_back(std::move(dt));
  }

  for (auto& dt : detached_tab) {
    notifications->detached_tab.push_back(std::move(dt));
  }

  return closed_all;
}

TabStripSelectionChange TabStripModel::SetSelection(
    ui::ListSelectionModel new_model,
    TabStripModelObserver::ChangeReason reason,
    bool triggered_by_other_operation) {
  TabStripSelectionChange selection;
  selection.old_model = selection_model();
  selection.old_tab = GetActiveTab();
  selection.old_contents = GetActiveWebContents();
  selection.new_model = new_model;
  selection.reason = reason;

  if (selection_model().active().has_value() &&
      new_model.active().has_value() &&
      selection_model().active().value() != new_model.active().value()) {
    if (GetActiveTab()->IsSplit() &&
        GetActiveTab()->GetSplit() ==
            GetSplitForTab(new_model.active().value())) {
      // When switching between two tabs in a split, neither enters the
      // background but one becomes deactivated.
      static_cast<tabs::TabModel*>(GetActiveTab())
          ->WillDeactivate(base::PassKey<TabStripModel>());
    } else {
      NotifyForegroundTabsWillEnterBackground();
    }
  }

  // This is done after notifying TabDeactivated() because caller can assume
  // that TabStripModel::active_index() would return the index for
  // |selection.old_contents|.
  selection_model_ = std::make_unique<ui::ListSelectionModel>(new_model);
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();

  if (!triggered_by_other_operation &&
      (selection.active_tab_changed() || selection.selection_changed())) {
    if (selection.active_tab_changed()) {
      // Start measuring the tab switch compositing time. This must be the first
      // thing in this block so that the start time is saved before any changes
      // that might affect compositing.
      if (selection.new_contents) {
        // Don't record the time if the old and new tabs are in the same split.
        CHECK(selection.new_tab);
        const auto new_split_id = selection.new_tab->GetSplit();
        const auto old_split_id =
            selection.old_tab ? selection.old_tab->GetSplit() : std::nullopt;
        if (!new_split_id || !old_split_id || new_split_id != old_split_id) {
          selection.new_contents->SetTabSwitchStartTime(
              base::TimeTicks::Now(),
              resource_coordinator::ResourceCoordinatorTabHelper::IsLoaded(
                  selection.new_contents));
        }
      }

      if (base::FeatureList::IsEnabled(media::kEnableTabMuting)) {
        // Show the in-product help dialog pointing users to the tab mute button
        // if the user backgrounds an audible tab.
        if (selection.old_contents &&
            selection.old_contents->IsCurrentlyAudible()) {
          if (auto* const user_ed =
                  BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
                      selection.old_contents)) {
            user_ed->MaybeShowFeaturePromo(
                feature_engagement::kIPHTabAudioMutingFeature);
          }
        }
      }
    }

    ValidateTabStripModel();

    TabStripModelChange change;
    OnChange(change, selection);
  }

  return selection;
}

void TabStripModel::SelectRelativeTab(TabRelativeDirection direction,
                                      TabStripUserGestureDetails detail) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (empty()) {
    return;
  }

  const int start_index = active_index();
  std::optional<tab_groups::TabGroupId> start_group =
      GetTabGroupForTab(start_index);

  // Ensure the active tab is not in a collapsed group so the while loop can
  // fallback on activating the active tab.
  DCHECK(!start_group.has_value() || !IsGroupCollapsed(start_group.value()));
  const int delta = direction == TabRelativeDirection::kNext ? 1 : -1;
  int index = (start_index + count() + delta) % count();
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  while (group.has_value() && IsGroupCollapsed(group.value())) {
    index = (index + count() + delta) % count();
    group = GetTabGroupForTab(index);
  }
  ActivateTabAt(index, detail);
}

void TabStripModel::MoveTabRelative(TabRelativeDirection direction) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(active_index() != kNoTab);
  const size_t active_tab_index = static_cast<size_t>(active_index());
  tabs::TabInterface* active_tab = GetTabAtIndex(active_tab_index);

  // The range of indices being moved. This will either be the active tab, or
  // all the tabs in the same split as the active tab. These are guaranteed to
  // be all have the same pinned state and group membership.
  // TODO: this needs to be updated for multi-selection.
  const gfx::Range moving_index_range =
      active_tab->IsSplit()
          ? GetIndexRangeOfSplit(active_tab->GetSplit().value())
          : gfx::Range{active_tab_index, active_tab_index + 1};

  // Calculate the target index the tabs needs to moved to. This will be the
  // destination index of the current tab at moving_index_range.start().
  int target_index = moving_index_range.start();
  int neighbor_index = direction == TabRelativeDirection::kNext
                           ? moving_index_range.end()
                           : moving_index_range.start() - 1;
  if (ContainsIndex(neighbor_index) &&
      IsTabPinned(moving_index_range.start()) == IsTabPinned(neighbor_index)) {
    int offset = 1;
    if (tabs::TabInterface* neighbor = GetTabAtIndex(neighbor_index);
        neighbor->IsSplit()) {
      offset = GetIndexRangeOfSplit(neighbor->GetSplit().value()).length();
    }
    target_index +=
        (direction == TabRelativeDirection::kNext ? 1 : -1) * offset;
  }

  std::optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(moving_index_range.start());
  // If the target index is the same as the current index, then the tab is at a
  // min/max boundary and being moved further in that direction. In that case,
  // the tab could still be ungrouped to move one more slot.
  std::optional<tab_groups::TabGroupId> target_group =
      (target_index == static_cast<int>(moving_index_range.start()))
          ? std::nullopt
          : GetTabGroupForTab(neighbor_index);

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  if (group_model_ && current_group != target_group) {
    if (current_group.has_value()) {
      target_index = moving_index_range.start();
      target_group = std::nullopt;
    } else if (target_group.has_value()) {
      // If the tab is at a group boundary and the group is collapsed, treat the
      // collapsed group as a tab and find the next available slot for the tab
      // to move to.
      const TabGroup* group = group_model_->GetTabGroup(target_group.value());
      if (group->visual_data()->is_collapsed()) {
        const gfx::Range tabs_in_group = group->ListTabs();
        target_index = direction == TabRelativeDirection::kNext
                           ? tabs_in_group.end() - moving_index_range.length()
                           : tabs_in_group.start();
        target_group = std::nullopt;
      } else {
        target_index = moving_index_range.start();
      }
    }
  }
  MoveTabsToIndexImpl(moving_index_range.ToIntVector(), target_index,
                      target_group);
}

std::pair<std::optional<int>, std::optional<int>>
TabStripModel::GetAdjacentTabsAfterSelectedMove(
    base::PassKey<DraggingTabsSession>,
    int destination_index) {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const std::vector<int> pinned_selected_indices = GetSelectedPinnedTabs();
  const std::vector<int> unpinned_selected_indices = GetSelectedUnpinnedTabs();
  std::pair<std::optional<int>, std::optional<int>> adjacent_tabs(std::nullopt,
                                                                  std::nullopt);

  // If `unpinned_selected_indices` is empty there are no adjacent tabs.
  if (unpinned_selected_indices.empty()) {
    return adjacent_tabs;
  }

  // The index should be clamped between the first possible unpinned tab
  // position and the end of the tabstrip.
  const int first_unpinned_selected_dst_index = std::clamp(
      destination_index + static_cast<int>(pinned_selected_indices.size()),
      pinned_tab_count,
      count() - static_cast<int>(unpinned_selected_indices.size()));

  // Get the left adjacent if the first unpinned selected is not in the start of
  // the unpinned container.
  if (first_unpinned_selected_dst_index > pinned_tab_count) {
    int non_selected_index = pinned_tab_count;
    for (int i = pinned_tab_count; i < count(); ++i) {
      if (!IsTabSelected(i)) {
        if (non_selected_index == first_unpinned_selected_dst_index - 1) {
          adjacent_tabs.first = i;
          break;
        }
        ++non_selected_index;
      }
    }
  } else {
    // Maybe the left adjacent is the last pinned tab.
    const int is_last_pinned_tab_selected =
        !pinned_selected_indices.empty() &&
        (destination_index + static_cast<int>(pinned_selected_indices.size()) -
             1 >=
         pinned_tab_count - 1);
    for (int i = pinned_tab_count - 1; i >= 0; i--) {
      if (IsTabSelected(i) == is_last_pinned_tab_selected) {
        adjacent_tabs.first = i;
        break;
      }
    }
  }

  const int last_unpinned_selected_dst_index =
      first_unpinned_selected_dst_index + unpinned_selected_indices.size() - 1;

  // Get the right adjacent if the last unpinned selected is not in the end of
  // the tabstrip.
  if (last_unpinned_selected_dst_index < count() - 1) {
    int non_selected_index = count() - 1;
    for (int i = count() - 1; i >= pinned_tab_count; i--) {
      if (!IsTabSelected(i)) {
        if (non_selected_index == last_unpinned_selected_dst_index + 1) {
          adjacent_tabs.second = i;
          break;
        }
        --non_selected_index;
      }
    }
  }

  return adjacent_tabs;
}

std::vector<int> TabStripModel::GetSelectedPinnedTabs() {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model().selected_indices();

  std::vector<int> indices;

  for (int selected_index : selected_indices) {
    if (selected_index < pinned_tab_count) {
      indices.push_back(selected_index);
    } else {
      // Since selected_indices are sorted, no more pinned tabs will be found
      break;
    }
  }

  return indices;
}

std::vector<int> TabStripModel::GetSelectedUnpinnedTabs() {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model().selected_indices();

  std::vector<int> indices;

  for (int selected_index : base::Reversed(selected_indices)) {
    if (selected_index >= pinned_tab_count) {
      // Insert to the start so it is in ascending order.
      indices.insert(indices.begin(), selected_index);
    } else {
      // Since selected_indices are sorted, no more unpinned tabs will be found
      break;
    }
  }

  return indices;
}

split_tabs::SplitTabId TabStripModel::AddToSplitImpl(
    split_tabs::SplitTabId split_id,
    const std::vector<int>& indices,
    int pivot_index,
    split_tabs::SplitTabVisualData visual_data,
    SplitTabChange::SplitTabAddReason reason) {
  std::vector<tabs::TabInterface*> tabs = {};
  for (int i : indices) {
    tabs::TabInterface* tab = GetTabAtIndex(i);
    CHECK(!tab->IsSplit());
    tabs.push_back(tab);
  }

  // Add the tabs to a split with the active index.
  MoveTabsAndSetPropertiesImpl(indices, pivot_index,
                               GetTabGroupForTab(pivot_index),
                               IsTabPinned(pivot_index));

  contents_data_->CreateSplit(split_id, tabs, visual_data);

  std::vector<std::pair<tabs::TabInterface*, int>> tabs_with_indices;
  for (tabs::TabInterface* tab : tabs) {
    tabs_with_indices.emplace_back(tab, GetIndexOfTab(tab));
  }

  bool add_to_selection = std::any_of(
      contents_data_->GetSplitTabCollection(split_id)->begin(),
      contents_data_->GetSplitTabCollection(split_id)->end(),
      [this](tabs::TabInterface* tab) {
        return IsTabSelected(GetIndexOfTab(tab)) || tab->IsActivated();
      });

  const ui::ListSelectionModel old_selection_model = selection_model();

  if (add_to_selection) {
    for (auto split_tab : tabs_with_indices) {
      selection_model_->AddIndexToSelection(split_tab.second);
    }
  }

  ValidateTabStripModel();

  if (old_selection_model != selection_model()) {
    TabStripSelectionChange selection(GetActiveTab(), old_selection_model);

    selection.new_model = selection_model();
    TabStripModelChange change;
    OnChange(change, selection);
  }

  NotifySplitTabCreated(split_id, tabs_with_indices, reason, visual_data);

  return split_id;
}

void TabStripModel::RemoveSplitImpl(
    split_tabs::SplitTabId split_id,
    SplitTabChange::SplitTabRemoveReason reason) {
  std::vector<std::pair<tabs::TabInterface*, int>> tabs_with_indices =
      GetTabsAndIndicesInSplit(split_id);

  contents_data_->Unsplit(split_id);

  const ui::ListSelectionModel old_selection_model = selection_model();

  for (const auto& [_, i] : tabs_with_indices) {
    if (selection_model().IsSelected(i) && i != active_index()) {
      selection_model_->RemoveIndexFromSelection(i);
    }
  }

  ValidateTabStripModel();

  // If there was an update to the selection model, notify observers.
  if (old_selection_model != selection_model()) {
    TabStripSelectionChange selection(GetActiveTab(), old_selection_model);
    selection.new_model = selection_model();
    TabStripModelChange change;
    OnChange(change, selection);
  }

  NotifySplitTabRemoved(split_id, tabs_with_indices, reason);
}

void TabStripModel::UpdateTabInSplitImpl(tabs::TabInterface* split_tab,
                                         int update_index,
                                         SplitUpdateType update_type) {
  CHECK(split_tab->IsSplit());
  const split_tabs::SplitTabId split_id = split_tab->GetSplit().value();

  std::vector<tabs::TabInterface*> tabs_to_split =
      GetSplitData(split_id)->ListTabs();
  split_tabs::SplitTabVisualData split_visual_data =
      *GetSplitData(split_id)->visual_data();
  const bool initial_split_active = split_tab->IsActivated();

  // Require that one of the tabs in the split must be active.
  CHECK(std::any_of(tabs_to_split.begin(), tabs_to_split.end(),
                    [](tabs::TabInterface* t) { return t->IsActivated(); }));

  // Remove `split_tab` from `tabs_to_split` as it will be replaced or swapped
  // out of the split and remove the active tab.
  std::erase_if(tabs_to_split, [split_tab](tabs::TabInterface* tab) {
    return tab == split_tab || tab->IsActivated();
  });

  // If the initial split isn't active, add the tab at `update_index` since it
  // will be added to the split.
  if (!initial_split_active) {
    tabs_to_split.push_back(GetTabAtIndex(update_index));
  }

  // This operation is a bulk operation and is done in multiple steps.
  // 1. Unsplit the collection so we can perform close and move to correct
  // index.
  // 2. Move the tab to replace to the correct index and make it active.
  // 3. Close the previous active tab (if we are replacing the tab).
  // 4. Re-split the other tabs that were a part of the split collection with
  // the new active tab (the initial tab at `update_index`)
  RemoveSplitImpl(split_id,
                  SplitTabChange::SplitTabRemoveReason::kSplitTabUpdated);

  if (update_type == SplitUpdateType::kReplace) {
    const int split_index = GetIndexOfTab(split_tab);
    MoveTabToIndexImpl(update_index, split_index, split_tab->GetGroup(),
                       split_tab->IsPinned(), initial_split_active);
    CloseWebContentsAt(GetIndexOfTab(split_tab),
                       TabCloseTypes::CLOSE_USER_GESTURE);
  } else {
    tabs::TabInterface* update_tab = GetTabAtIndex(update_index);
    std::optional<tab_groups::TabGroupId> initial_split_group =
        split_tab->GetGroup();
    const bool initial_split_pinned = split_tab->IsPinned();
    const int split_index = GetIndexOfTab(split_tab);

    // The `split_tab` will be replaced in the split so notify observers that it
    // will be moving to the background.
    if (split_tab->IsActivated()) {
      static_cast<tabs::TabModel*>(split_tab)->WillDeactivate(
          base::PassKey<TabStripModel>());
    }
    static_cast<tabs::TabModel*>(split_tab)->WillBecomeHidden(
        base::PassKey<TabStripModel>());

    // Move the split index first so the group is not possibly destroyed at the
    // update index. This can happen when the update index is the only member of
    // a group. Note that the split tab cannot be the only member of a group
    // since it is a split tab.
    //
    // Adjust the `final_index` location by shifting it one towards the
    // `update_index`. When `split_index` and `update_index` are adjacent, the
    // second `MoveTabToIndexImpl` can be a no-op if the tab at `update_index`
    // isn't grouped or pinned. This results in the active state not being
    // updated properly. Instead make the first move be to the same location by
    // shifting the indices so the tab at `update_index` can pick up any
    // pin/group state changes so the second move is guaranteed to apply the
    // selection update.
    MoveTabToIndexImpl(
        split_index,
        update_index > split_index ? update_index - 1 : update_index + 1,
        update_tab->GetGroup(), update_tab->IsPinned(), false);

    MoveTabToIndexImpl(GetIndexOfTab(update_tab), split_index,
                       initial_split_group, initial_split_pinned,
                       initial_split_active);
  }

  std::vector<int> split_indices;
  for (tabs::TabInterface* tab : tabs_to_split) {
    split_indices.emplace_back(GetIndexOfTab(tab));
  }

  // Insert the active index into the sorted `indices`.
  auto position =
      lower_bound(split_indices.begin(), split_indices.end(), active_index());
  split_indices.insert(position, active_index());

  AddToSplitImpl(split_id, split_indices, active_index(), split_visual_data,
                 SplitTabChange::SplitTabAddReason::kSplitTabUpdated);

  split_tabs::LogSplitViewUpdatedUKM(this, split_id);
}

void TabStripModel::AddToNewGroupImpl(
    const std::vector<int>& indices,
    const tab_groups::TabGroupId& new_group,
    std::optional<tab_groups::TabGroupVisualData> visual_data) {
  if (!group_model_) {
    return;
  }

  // Verify that the group id is not being used by any existing group. Group ids
  // are generated randomly but a conflict should be almost impossible in
  // practice.
  DCHECK([&]() {
    for (const tabs::TabInterface* tab : *this) {
      if (tab->GetGroup() == new_group) {
        return false;
      }
    }
    return true;
  }());

  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, new_group,
          tab_groups::TabGroupVisualData(
              std::u16string(),
              group_model_->GetNextColor(base::PassKey<TabStripModel>())));
  group_model_->AddTabGroup(group_collection->GetTabGroup(),
                            base::PassKey<TabStripModel>());
  contents_data_->CreateTabGroup(std::move(group_collection));

  // Find a destination for the first tab that's not pinned or inside another
  // group. We will stack the rest of the tabs up to its right.
  int destination_index = -1;
  for (int i = indices[0]; i <= count(); i++) {
    // Grouping at the end of the tabstrip is always valid.
    if (!ContainsIndex(i)) {
      destination_index = i;
      break;
    }

    // Grouping in the middle of pinned tabs is never valid.
    if (IsTabPinned(i)) {
      continue;
    }

    // Otherwise, grouping is valid if the destination is not in the middle of a
    // different group.
    std::optional<tab_groups::TabGroupId> destination_group =
        GetTabGroupForTab(i);
    if (!destination_group.has_value() ||
        destination_group != GetTabGroupForTab(indices[0])) {
      destination_index = i;
      break;
    }
  }

  MoveTabsAndSetPropertiesImpl(indices, destination_index, new_group, false);

  // Excluding the active tab, deselect all tabs being added to the group.
  // See crbug/1301846 for more info.
  const gfx::Range tab_indices =
      group_model()->GetTabGroup(new_group)->ListTabs();
  for (auto index = tab_indices.start(); index < tab_indices.end(); ++index) {
    if (active_index() != static_cast<int>(index) && IsTabSelected(index)) {
      DeselectTabAt(index);
    }
  }
}

void TabStripModel::AddToExistingGroupImpl(const std::vector<int>& indices,
                                           const tab_groups::TabGroupId& group,
                                           const bool add_to_end) {
  if (!group_model_) {
    return;
  }

  // Do nothing if the "existing" group can't be found. This would only happen
  // if the existing group is closed programmatically while the user is
  // interacting with the UI - e.g. if a group close operation is started by an
  // extension while the user clicks "Add to existing group" in the context
  // menu.
  // If this happens, the browser should not crash. So here we just make it a
  // no-op, since we don't want to create unintended side effects in this rare
  // corner case.
  if (!group_model_->ContainsTabGroup(group)) {
    return;
  }

  const TabGroup* group_object = group_model_->GetTabGroup(group);

  tabs::TabInterface* first_tab_in_group = group_object->GetFirstTab();
  CHECK(first_tab_in_group);
  int first_tab_index = GetIndexOfTab(first_tab_in_group);

  tabs::TabInterface* last_tab_in_group = group_object->GetLastTab();
  int last_tab_index = GetIndexOfTab(last_tab_in_group);

  // Split |new_indices| into |tabs_left_of_group| and |tabs_right_of_group| to
  // be moved to proper destination index. Directly set the group for indices
  // that are inside the group.
  std::vector<int> tabs_left_of_group;
  std::vector<int> tabs_right_of_group;
  for (int index : indices) {
    if (index < first_tab_index) {
      tabs_left_of_group.push_back(index);
    } else if (index > last_tab_index) {
      tabs_right_of_group.push_back(index);
    }
  }

  if (add_to_end) {
    std::vector<int> all_tabs = tabs_left_of_group;
    all_tabs.insert(all_tabs.end(), tabs_right_of_group.begin(),
                    tabs_right_of_group.end());
    MoveTabsAndSetPropertiesImpl(all_tabs, last_tab_index + 1, group, false);
  } else {
    MoveTabsAndSetPropertiesImpl(tabs_left_of_group, first_tab_index, group,
                                 false);
    MoveTabsAndSetPropertiesImpl(tabs_right_of_group, last_tab_index + 1, group,
                                 false);
  }
}

void TabStripModel::MoveTabsAndSetPropertiesImpl(
    const std::vector<int>& indices,
    int destination_index,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  if (!group_model_) {
    return;
  }

  static const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>({tabs::TabCollection::Type::SPLIT});
  // TabStripCollection::MoveTabsRecursive moves tabs to the destination index
  // after the tabs are removed, so adjust `destination_index` by subtracting
  // the number of tabs to the left of it.
  size_t num_tabs_to_left_of_destination = 0;
  for (auto i : indices) {
    if (i >= destination_index) {
      break;
    }
    num_tabs_to_left_of_destination++;
  }
  destination_index -= num_tabs_to_left_of_destination;

  MoveTabsWithNotifications(
      indices, destination_index,
      base::BindOnce(&tabs::TabStripCollection::MoveTabsRecursive,
                     base::Unretained(contents_data_.get()), indices,
                     destination_index, group, pinned, kRetainCollectionTypes));
}

void TabStripModel::AddToReadLaterImpl(const std::vector<int>& indices) {
  std::vector<WebContents*> web_contentses = GetWebContentsesByIndices(indices);
  delegate_->AddToReadLater(web_contentses);
}

void TabStripModel::InsertTabAtIndexImpl(
    std::unique_ptr<tabs::TabModel> tab_model,
    int index,
    std::optional<tab_groups::TabGroupId> group,
    bool pin,
    bool active) {
  tabs::TabModel* const tab_ptr = tab_model.get();

  if (std::optional<split_tabs::SplitTabId> split_id =
          InsertionBreaksSplitContiguity(index);
      split_id.has_value()) {
    RemoveSplitImpl(split_id.value(),
                    SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
  }

  tabs::TabInterface* old_active_tab = GetActiveTab();
  contents_data_->AddTabRecursive(std::move(tab_model), index, group, pin);

  // Update selection model and send the notification.
  selection_model_->IncrementFrom(index);

  // Start computing selection change after updating the indices in
  // `selection_model_`.
  TabStripSelectionChange selection(old_active_tab, selection_model());
  if (active) {
    ui::ListSelectionModel new_model(*selection_model_.get());
    SetSelectedIndex(&new_model, index);
    SetSelection(std::move(new_model),
                 TabStripModelObserver::CHANGE_REASON_NONE,
                 /*triggered_by_other_operation=*/true);
  }

  ValidateTabStripModel();

  tab_ptr->DidInsert(base::PassKey<TabStripModel>());

  selection.new_model = selection_model();
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();
  TabStripModelChange::Insert insert;
  insert.contents.push_back({tab_ptr, tab_ptr->GetContents(), index});
  TabStripModelChange change(std::move(insert));
  OnChange(change, selection);

  if (group_model_ && group.has_value()) {
    TabGroupStateChanged(index, tab_ptr, std::nullopt, group);
  }
}

std::unique_ptr<tabs::TabModel> TabStripModel::RemoveTabFromIndexImpl(
    int index,
    tabs::TabInterface::DetachReason tab_detach_reason) {
  tabs::TabModel* const tab = GetTabModelAtIndex(index);
  const std::optional<tab_groups::TabGroupId> old_group = tab->GetGroup();
  std::optional<int> next_selected_index = DetermineNewSelectedIndex(tab);
  const bool removed_tab_is_split = tab->IsSplit();
  if (removed_tab_is_split) {
    RemoveSplitImpl(tab->GetSplit().value(),
                    SplitTabChange::SplitTabRemoveReason::kSplitTabRemoved);
  }

  if (tab_detach_reason == tabs::TabInterface::DetachReason::kDelete) {
    tab->DestroyTabFeatures();
  }

  // Remove the tab.
  std::unique_ptr<tabs::TabModel> old_data =
      base::WrapUnique(static_cast<tabs::TabModel*>(
          contents_data_->RemoveTabAtIndexRecursive(index).release()));

  if (empty()) {
    selection_model_->Clear();
  } else {
    int old_active = active_index();
    selection_model_->DecrementFrom(index);
    if (index == old_active) {
      if (removed_tab_is_split) {
        // If the removed tab was part of a split, we should go to the first tab
        // in the split.
        selection_model_->set_active(next_selected_index);
        selection_model_->set_anchor(next_selected_index);
        if (next_selected_index.has_value()) {
          selection_model_->AddIndexToSelection(next_selected_index.value());
        }
      } else if (!selection_model_->empty()) {
        // The active tab was removed, but there is still something selected.
        // Move the active and anchor to the first selected index.
        selection_model_->set_active(
            *selection_model_->selected_indices().begin());
        selection_model_->set_anchor(selection_model_->active());
      } else {
        // The active tab was removed and nothing is selected. Reset the
        // selection and send out notification.
        SetSelectedIndex(selection_model_.get(), next_selected_index.value());
      }
    }
  }

  ValidateTabStripModel();

  if (group_model_ && old_group) {
    TabGroupStateChanged(index, tab, old_group, std::nullopt);
  }

  return old_data;
}

void TabStripModel::MoveTabToIndexImpl(
    int initial_index,
    int final_index,
    const std::optional<tab_groups::TabGroupId> group,
    bool pin,
    bool select_after_move) {
  CHECK(ContainsIndex(initial_index));
  CHECK_LT(initial_index, count());
  CHECK_LT(final_index, count());

  tabs::TabInterface* const tab = GetTabAtIndex(initial_index);
  const bool initial_pinned_state = tab->IsPinned();
  const std::optional<tab_groups::TabGroupId> initial_group = tab->GetGroup();

  // If nothing has changed, noop.
  if (initial_index == final_index && group == initial_group &&
      initial_pinned_state == pin) {
    return;
  }

  MaybeRemoveSplitsForMove(initial_index, final_index, group, pin);

  // If the tab still has a split id after MaybeRemoveSplitsForMove, then it
  // must be a move within a split.
  bool move_within_split =
      initial_index != final_index && tab->GetSplit().has_value();
  std::vector<std::pair<tabs::TabInterface*, int>> initial_split_tabs;
  if (move_within_split) {
    initial_split_tabs = GetTabsAndIndicesInSplit(tab->GetSplit().value());
  }

  if (initial_index != final_index) {
    FixOpeners(initial_index);
  }

  TabStripSelectionChange selection(GetActiveTab(), selection_model());
  if (move_within_split) {
    int index_of_first_tab_in_split = initial_split_tabs[0].second;
    CHECK(final_index >= index_of_first_tab_in_split);
    contents_data_->GetSplitTabCollection(tab->GetSplit().value())
        ->MoveTab(tab, final_index - index_of_first_tab_in_split);
  } else {
    contents_data_->MoveTabRecursive(initial_index, final_index, group, pin);
  }

  UpdateSelectionModelForMove(initial_index, final_index, select_after_move);

  ValidateTabStripModel();

  selection.new_model = selection_model();
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();

  // Send all the notifications.
  if (initial_index != final_index) {
    SendMoveNotificationForTab(initial_index, final_index, tab, selection);
  }

  if (move_within_split) {
    NotifySplitTabContentsUpdated(
        tab->GetSplit().value(), initial_split_tabs,
        GetTabsAndIndicesInSplit(tab->GetSplit().value()));
  }

  if (initial_pinned_state != tab->IsPinned()) {
    for (auto& observer : observers_) {
      observer.TabPinnedStateChanged(this, tab->GetContents(), final_index);
    }
  }

  if (group_model_) {
    if (initial_group != tab->GetGroup()) {
      TabGroupStateChanged(final_index, tab, initial_group, tab->GetGroup());
    }
  }

}

void TabStripModel::MoveTabsToIndexImpl(
    const std::vector<int>& tab_indices,
    int destination_index,
    const std::optional<tab_groups::TabGroupId> group) {
  if (tab_indices.empty()) {
    return;
  }

  static const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>(
          {tabs::TabCollection::Type::SPLIT, tabs::TabCollection::Type::GROUP});

  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const bool pin = IsTabPinned(tab_indices[0]);
  const bool all_tabs_pinned = std::all_of(
      tab_indices.begin(), tab_indices.end(),
      [pinned_tab_count](int index) { return index < pinned_tab_count; });
  const bool all_tabs_unpinned = std::all_of(
      tab_indices.begin(), tab_indices.end(),
      [pinned_tab_count](int index) { return index >= pinned_tab_count; });

  CHECK(all_tabs_pinned || all_tabs_unpinned);
  CHECK(std::ranges::is_sorted(tab_indices));

  // Update `contents_data`.
  MoveTabsWithNotifications(
      tab_indices, destination_index,
      base::BindOnce(&tabs::TabStripCollection::MoveTabsRecursive,
                     base::Unretained(contents_data_.get()), tab_indices,
                     destination_index, group, pin, kRetainCollectionTypes));
}

void TabStripModel::TabGroupStateChanged(
    int index,
    tabs::TabInterface* tab,
    const std::optional<tab_groups::TabGroupId> initial_group,
    const std::optional<tab_groups::TabGroupId> new_group) {
  if (!group_model_) {
    return;
  }

  if (initial_group == new_group) {
    return;
  }

  if (initial_group.has_value()) {
    TabGroup* tab_group = group_model_->GetTabGroup(initial_group.value());
    tab_group->RemoveTab();

    // Send the observation
    for (auto& observer : observers_) {
      observer.TabGroupedStateChanged(this, initial_group, std::nullopt, tab,
                                      index);
    }

    // If the group model must be deleted, then do that at this point.
    if (tab_group->IsEmpty()) {
      if (focused_group_ == initial_group) {
        SetFocusedGroup(std::nullopt);
      }
      NotifyTabGroupClosed(initial_group.value());
      group_model_->RemoveTabGroup(initial_group.value(),
                                   base::PassKey<TabStripModel>());
      contents_data_->CloseDetachedTabGroup(initial_group.value());
    }
  }

  if (new_group.has_value()) {
    // Use IsEmpty() method as it relies on the tab_count_ maintained by  the
    // TabGroup object. Any method that relies on the model for this would be
    // wrong since the model is already updated.
    const bool is_group_empty =
        group_model_->GetTabGroup(new_group.value())->IsEmpty();

    // Update the group model.
    AddTabToGroupModel(new_group.value());

    // Send the observation
    for (auto& observer : observers_) {
      observer.TabGroupedStateChanged(this, std::nullopt, new_group, tab,
                                      index);
    }

    // TODO(398256328): Look into replacing the empty visual change with
    // providing the right initial value or migrating clients to working with
    // TabGroupChange::kCreated.
    if (is_group_empty) {
      TabGroupChange::VisualsChange visuals;
      NotifyTabGroupVisualsChanged(new_group.value(), visuals);
    }
  }
}

void TabStripModel::AddTabToGroupModel(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }
  TabGroup* tab_group = group_model_->GetTabGroup(group);
  if (tab_group->IsEmpty()) {
    NotifyTabGroupCreated(group);
  }
  tab_group->AddTab();
}

void TabStripModel::ValidateTabStripModel() {
  if (empty()) {
    return;
  }

  CHECK(selection_model().active().has_value());
  CHECK(ContainsIndex(selection_model().active().value()));
  CHECK(GetTabAtIndex(selection_model().active().value()));

  // Check if the selected tab indices are valid.
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model().selected_indices();

  std::set<split_tabs::SplitTabId> selected_splits;
  for (auto selection : selected_indices) {
    // Check if the selected tab indices are valid.
    const tabs::TabInterface* tab = GetTabAtIndex(selection);
    CHECK(tab);

    if (tab->IsSplit()) {
      selected_splits.insert(tab->GetSplit().value());
    }
  }

  // For all splits that have at least one tab selected, check that all tabs are
  // selected.
  for (split_tabs::SplitTabId split_id : selected_splits) {
    std::vector<std::pair<tabs::TabInterface*, int>> tabs_in_split =
        GetTabsAndIndicesInSplit(split_id);
    CHECK(std::all_of(tabs_in_split.begin(), tabs_in_split.end(),
                      [&](std::pair<tabs::TabInterface*, int> tab) {
                        return IsTabSelected(tab.second);
                      }));
  }

  contents_data_->ValidateData();

  // Send the notifications for the root collection.
  contents_data_->DispatchPendingNotifications();
}

void TabStripModel::SendMoveNotificationForTab(
    int index,
    int to_position,
    tabs::TabInterface* tab,
    const TabStripSelectionChange& selection_change) {
  TabStripModelChange::Move move;
  move.tab = tab;
  move.contents = tab->GetContents();
  move.from_index = index;
  move.to_index = to_position;
  TabStripModelChange change(move);
  OnChange(change, selection_change);
}

void TabStripModel::UpdateSelectionModelForMove(int initial_index,
                                                int final_index,
                                                bool select_after_move) {
  if (initial_index == final_index) {
    return;
  }

  selection_model_->Move(initial_index, final_index, 1);
  if (!selection_model().IsSelected(final_index) && select_after_move) {
    SetSelectedIndex(selection_model_.get(), final_index);
  }
}

void TabStripModel::UpdateSelectionModelForMoves(
    const std::vector<int>& tab_indices,
    int destination_index) {
  const std::vector<std::pair<int, int>> moved_indices =
      CalculateIncrementalTabMoves(tab_indices, destination_index);

  for (std::pair<int, int> move : moved_indices) {
    if (move.first != move.second) {
      selection_model_->Move(move.first, move.second, 1);
    }
  }
}

void TabStripModel::SetSelectedIndex(ui::ListSelectionModel* selection,
                                     int index) {
  selection->SetSelectedIndex(index);

  if (std::optional<split_tabs::SplitTabId> split_id = GetSplitForTab(index);
      split_id.has_value()) {
    gfx::Range index_range = GetIndexRangeOfSplit(split_id.value());
    selection->AddIndexRangeToSelection(index_range.start(),
                                        index_range.end() - 1);
  }
}

std::pair<int, int> TabStripModel::GetSelectionRangeFromAnchorToIndex(
    int index) {
  if (!selection_model().anchor().has_value()) {
    if (std::optional<split_tabs::SplitTabId> split_id = GetSplitForTab(index);
        split_id.has_value()) {
      gfx::Range index_range = GetIndexRangeOfSplit(split_id.value());
      return std::pair(index_range.start(), index_range.end() - 1);
    } else {
      return std::pair(index, index);
    }
  }

  const int anchor_index = static_cast<int>(selection_model().anchor().value());

  // If the start index is part of a split, find the leftmost index in that
  // split.
  int start_index = std::min(index, anchor_index);
  if (std::optional<split_tabs::SplitTabId> split_id =
          GetSplitForTab(start_index);
      split_id.has_value()) {
    start_index = GetIndexRangeOfSplit(split_id.value()).GetMin();
  }

  // If the end index is part of a split, find the rightmost index in that
  // split.
  int end_index = std::max(index, anchor_index);
  if (std::optional<split_tabs::SplitTabId> split_id =
          GetSplitForTab(end_index);
      split_id.has_value()) {
    end_index = GetIndexRangeOfSplit(split_id.value()).GetMax() - 1;
  }

  return std::pair(start_index, end_index);
}

std::vector<std::pair<int, int>> TabStripModel::CalculateIncrementalTabMoves(
    const std::vector<int>& tab_indices,
    int destination_index) const {
  std::vector<std::pair<int, int>> source_and_target_indices_to_move_left;
  std::vector<std::pair<int, int>> source_and_target_indices_to_move_right;

  // We want a sequence of moves that moves each tab directly from its
  // initial index to its final index. This is possible if and only if
  // every move maintains the same relative order of the moving tabs.
  // We do this by splitting the tabs based on which direction they're
  // moving, then moving them in the correct order within each group.
  int tab_destination_index = destination_index;
  for (int source_index : tab_indices) {
    if (source_index < tab_destination_index) {
      source_and_target_indices_to_move_right.emplace_back(
          source_index, tab_destination_index++);
    } else {
      source_and_target_indices_to_move_left.emplace_back(
          source_index, tab_destination_index++);
    }
  }

  std::vector<std::pair<int, int>> moved_indices;
  std::copy(source_and_target_indices_to_move_right.rbegin(),
            source_and_target_indices_to_move_right.rend(),
            std::back_inserter(moved_indices));

  std::copy(source_and_target_indices_to_move_left.begin(),
            source_and_target_indices_to_move_left.end(),
            std::back_inserter(moved_indices));

  return moved_indices;
}

std::vector<TabStripModel::MoveNotification>
TabStripModel::PrepareTabsToMoveToIndex(const std::vector<int>& tab_indices,
                                        int destination_index) {
  const std::vector<std::pair<int, int>> moved_indices =
      CalculateIncrementalTabMoves(tab_indices, destination_index);
  std::vector<MoveNotification> notifications;

  ui::ListSelectionModel old_selection_model = selection_model();
  for (std::pair<int, int> move : moved_indices) {
    if (move.first != move.second) {
      FixOpeners(move.first);
    }

    // Update the `old_selection_model`
    TabStripSelectionChange selection;
    if (move.first == move.second) {
      selection = TabStripSelectionChange();
    } else {
      selection = TabStripSelectionChange(GetActiveTab(), old_selection_model);
      old_selection_model.Move(move.first, move.second, 1);
      selection.new_model = old_selection_model;
    }

    const tabs::TabInterface* const tab = GetTabAtIndex(move.first);
    const MoveNotification notification = {move.first, tab->GetGroup(),
                                           tab->IsPinned(), tab, selection};
    notifications.push_back(notification);
  }

  return notifications;
}

void TabStripModel::SetTabsPinned(std::vector<int> indices, bool pinned) {
  // `indices` are given in ascending order. If pinning, process the indices as
  // is, since when moving the tab at `index` to the left, this will not change
  // the tabs that are pointed to by indices larger than `index`. Similarly, if
  // unpinning, process the indices in descending order.
  if (!pinned) {
    std::ranges::reverse(indices);
  }

  // When we see a tab that is part of a split, do not move it until we look
  // forward and see if all the tabs in the split are in indices. If so, move
  // the whole split, otherwise move the tabs individually. Splits are
  // contiguous, so once we stop seeing a split, we will not see it again,
  // therefore we dont have to worry about processing the same split twice.
  size_t next_i;
  for (size_t i = 0; i < indices.size(); i = next_i) {
    next_i = i + 1;

    int index = indices[i];
    if (IsTabPinned(index) == pinned) {
      continue;
    }

    tabs::TabInterface* tab = GetTabAtIndex(index);

    if (tab->IsSplit()) {
      tabs::SplitTabCollection* split =
          contents_data_->GetSplitTabCollection(tab->GetSplit().value());

      // Fast forward until we are no longer in the split.
      while (next_i < indices.size() &&
             next_i < i + split->TabCountRecursive() &&
             GetTabAtIndex(indices[next_i])->GetSplit() == tab->GetSplit()) {
        next_i++;
      }

      if (next_i == i + split->TabCountRecursive()) {
        SetSplitPinnedImpl(split, pinned);
      } else {
        for (size_t j = i; j < next_i; j++) {
          SetTabPinnedImpl(indices[j], pinned);
        }
      }
    } else {
      SetTabPinnedImpl(index, pinned);
    }
  }
}

int TabStripModel::SetTabPinnedImpl(int index, bool pinned) {
  const int final_index =
      pinned ? IndexOfFirstNonPinnedTab() : IndexOfFirstNonPinnedTab() - 1;

  MoveTabToIndexImpl(index, final_index, std::nullopt, pinned, false);
  return final_index;
}

void TabStripModel::SetSplitPinnedImpl(tabs::SplitTabCollection* split,
                                       bool pinned) {
  static const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>({tabs::TabCollection::Type::SPLIT});
  std::vector<tabs::TabInterface*> tabs = split->GetTabsRecursive();
  std::vector<int> tab_indices = {};
  for (size_t index = GetIndexOfTab(tabs[0]); tabs::TabInterface* _ : tabs) {
    tab_indices.push_back(index++);
  }
  const int destination_index = pinned
                                    ? IndexOfFirstNonPinnedTab()
                                    : IndexOfFirstNonPinnedTab() - tabs.size();

  MoveTabsWithNotifications(
      tab_indices, destination_index,
      base::BindOnce(&tabs::TabStripCollection::MoveTabsRecursive,
                     base::Unretained(contents_data_.get()), tab_indices,
                     destination_index, std::nullopt, pinned,
                     kRetainCollectionTypes));
}

void TabStripModel::MoveTabsWithNotifications(
    std::vector<int> tab_indices,
    int destination_index,
    base::OnceClosure execute_tabs_move_operation) {
  const std::vector<MoveNotification> notifications =
      PrepareTabsToMoveToIndex(tab_indices, destination_index);

  std::move(execute_tabs_move_operation).Run();

  UpdateSelectionModelForMoves(tab_indices, destination_index);

  ValidateTabStripModel();

  for (const auto& notification : notifications) {
    const int final_index = GetIndexOfTab(notification.tab);
    tabs::TabInterface* tab = GetTabAtIndex(final_index);
    if (notification.initial_index != final_index) {
      SendMoveNotificationForTab(notification.initial_index, final_index, tab,
                                 notification.selection_change);
    }

    if (group_model_) {
      if (notification.intial_group != tab->GetGroup()) {
        TabGroupStateChanged(final_index, tab, notification.intial_group,
                             tab->GetGroup());
      }
    }

    if (notification.initial_pinned != tab->IsPinned()) {
      for (auto& observer : observers_) {
        observer.TabPinnedStateChanged(this, tab->GetContents(), final_index);
      }
    }
  }
}

// Sets the sound content setting for each site at the |indices|.
void TabStripModel::SetSitesMuted(const std::vector<int>& indices,
                                  bool mute) const {
  for (int tab_index : indices) {
    content::WebContents* web_contents = GetWebContentsAt(tab_index);
    GURL url = web_contents->GetLastCommittedURL();

    // `GetLastCommittedURL` could return an empty URL if no navigation has
    // occurred yet.
    if (url.is_empty()) {
      continue;
    }

    if (url.SchemeIs(content::kChromeUIScheme)) {
      // chrome:// URLs don't have content settings but can be muted, so just
      // mute the WebContents.
      SetTabAudioMuted(web_contents, mute,
                       TabMutedReason::kContentSettingChrome, std::string());
    } else {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      HostContentSettingsMap* map =
          HostContentSettingsMapFactory::GetForProfile(profile);
      ContentSetting setting =
          mute ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;

      // The goal is to only add the site URL to the exception list if
      // the request behavior differs from the default value or if there is an
      // existing less specific rule (i.e. wildcards) in the exception list.
      if (!profile->IsIncognitoProfile()) {
        // Using default setting value below clears the setting from the
        // exception list for the site URL if it exists.
        map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                           CONTENT_SETTING_DEFAULT);

        // If the current setting matches the desired setting after clearing the
        // site URL from the exception list we can simply skip otherwise we
        // will add the site URL to the exception list.
        if (setting ==
            map->GetContentSetting(url, url, ContentSettingsType::SOUND)) {
          continue;
        }
      }
      // Adds the site URL to the exception list for the setting.
      map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                         setting);
    }
  }
}

void TabStripModel::FixOpeners(int index) {
  tabs::TabModel* old_tab = GetTabModelAtIndex(index);
  tabs::TabInterface* new_opener = old_tab ? old_tab->opener() : nullptr;

  for (tabs::TabInterface* tab : *this) {
    auto* tab_model = static_cast<tabs::TabModel*>(tab);
    if (tab_model->opener() != old_tab) {
      continue;
    }

    // Ensure a tab isn't its own opener.
    tab_model->set_opener(new_opener == tab_model ? nullptr : new_opener);
  }

  // Sanity check that none of the tabs' openers refer |old_tab| or
  // themselves.
  DCHECK([&]() {
    return std::none_of(begin(), end(), [&](tabs::TabInterface* tab) {
      tabs::TabInterface* opener = static_cast<tabs::TabModel*>(tab)->opener();
      return opener == old_tab || opener == tab;
    });
  }());
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetGroupToAssign(
    int index,
    int to_position) {
  CHECK(ContainsIndex(index));
  CHECK(ContainsIndex(to_position));

  tabs::TabInterface* tab_to_move = GetTabAtIndex(index);

  if (!group_model_) {
    return std::nullopt;
  }

  std::optional<tab_groups::TabGroupId> new_left_group;
  std::optional<tab_groups::TabGroupId> new_right_group;

  if (to_position > index) {
    new_left_group = GetTabGroupForTab(to_position);
    new_right_group = GetTabGroupForTab(to_position + 1);
  } else if (to_position < index) {
    new_left_group = GetTabGroupForTab(to_position - 1);
    new_right_group = GetTabGroupForTab(to_position);
  }

  if (tab_to_move->GetGroup() != new_left_group &&
      tab_to_move->GetGroup() != new_right_group) {
    if (new_left_group == new_right_group && new_left_group.has_value()) {
      // The tab is in the middle of an existing group, so add it to that group.
      return new_left_group;
    } else if (tab_to_move->GetGroup().has_value() &&
               group_model_->GetTabGroup(tab_to_move->GetGroup().value())
                       ->tab_count() > 1) {
      // The tab is between groups and its group is non-contiguous, so clear
      // this tab's group.
      return std::nullopt;
    }
  }

  return tab_to_move->GetGroup();
}

std::optional<const tab_groups::TabGroupId> TabStripModel::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle) const {
  return contents_data_->FindGroupIdFor(collection_handle,
                                        base::PassKey<TabStripModel>());
}

int TabStripModel::GetTabIndexAfterClosing(int index,
                                           const gfx::Range& block_tabs) const {
  CHECK(!block_tabs.Contains(gfx::Range(index)));

  const int last_tab_in_block = static_cast<int>(block_tabs.end() - 1);

  if (index > last_tab_in_block) {
    index = index - static_cast<int>(block_tabs.length());
    CHECK(index >= 0);
  }

  return index;
}

void TabStripModel::OnActiveTabChanged(
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || empty()) {
    return;
  }

  const tabs::TabInterface* const old_tab = selection.old_tab;
  const tabs::TabInterface* const new_tab = selection.new_tab;
  const tabs::TabInterface* old_opener = nullptr;
  int reason = selection.reason;

  if (new_tab->GetGroup()) {
    group_model_->OnTabGroupActivated(*(new_tab->GetGroup()),
                                      base::PassKey<TabStripModel>());
  }

  if (old_tab) {
    const int index = GetIndexOfTab(old_tab);
    if (index != TabStripModel::kNoTab) {
      // When switching away from a tab, the tab preview system may want to
      // capture an updated preview image. This must be done before any changes
      // are made to the old contents, and while the contents are still visible.
      //
      // It's possible this could be done with a separate TabStripModelObserver,
      // but then it would be possible for a different observer to jump in front
      // and modify the WebContents, so for now, do it here.
      auto* const thumbnail_helper =
          ThumbnailTabHelper::FromWebContents(old_tab->GetContents());
      if (thumbnail_helper) {
        thumbnail_helper->CaptureThumbnailOnTabBackgrounded();
      }

      old_opener = GetOpenerOfTabAt(index);

      // Forget the opener relationship if it needs to be reset whenever the
      // active tab changes (see comment in TabStripModel::AddWebContents, where
      // the flag is set).
      if (GetTabModelAtIndex(index)->reset_opener_on_active_tab_change()) {
        ForgetOpener(old_tab->GetContents());
      }
    }
  }
  DCHECK(selection.new_model.active().has_value());
  const tabs::TabInterface* const new_opener =
      GetOpenerOfTabAt(selection.new_model.active().value());

  if ((reason & TabStripModelObserver::CHANGE_REASON_USER_GESTURE) &&
      new_opener != old_opener &&
      ((old_tab == nullptr && new_opener == nullptr) ||
       new_opener != old_tab) &&
      ((new_tab == nullptr && old_opener == nullptr) ||
       old_opener != new_tab)) {
    ForgetAllOpeners();
  }
}

bool TabStripModel::PolicyAllowsTabClosing(
    content::WebContents* contents) const {
  if (!contents) {
    return true;
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebContents(contents);
  // Can be null if there is no tab helper or app id.
  const webapps::AppId* app_id = web_app::WebAppTabHelper::GetAppId(contents);
  if (!app_id) {
    return true;
  }

  return !delegate()->IsForWebApp() ||
         !provider->policy_manager().IsPreventCloseEnabled(*app_id);
}

int TabStripModel::DetermineInsertionIndex(ui::PageTransition transition,
                                           bool foreground) {
  int tab_count = count();
  if (!tab_count) {
    return 0;
  }

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
      active_index() != -1) {
    if (foreground) {
      // If the page was opened in the foreground by a link click in another
      // tab, insert it adjacent to the tab that opened that link.
      return active_index() + 1;
    }
    content::WebContents* const opener = GetActiveWebContents();
    // Figure out the last tab opened by the current tab.
    const int index = GetIndexOfLastWebContentsOpenedBy(opener, active_index());
    // If no such tab exists, simply place next to the current tab.
    if (index == TabStripModel::kNoTab) {
      return active_index() + 1;
    }

    // Normally we'd add the tab immediately after the most recent tab
    // associated with `opener`. However, if there is a group discontinuity
    // between the active tab and where we'd like to place the tab, we'll place
    // it just before the discontinuity instead (see crbug.com/1246421).
    const auto opener_group = GetTabGroupForTab(active_index());
    for (int i = active_index() + 1; i <= index; ++i) {
      // Insert before the first tab that differs in group.
      if (GetTabGroupForTab(i) != opener_group) {
        return i;
      }
    }
    // If there is no discontinuity, add after the last tab already associated
    // with the opener.
    return index + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return count();
}

void TabStripModel::GroupCloseStopped(const tab_groups::TabGroupId& group) {
  delegate_->GroupCloseStopped(group);

  gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  RemoveFromGroup(tabs_in_group.ToIntVector());
}

std::optional<int> TabStripModel::DetermineNewSelectedIndex(
    std::variant<tabs::TabInterface*, tabs::TabCollection*> tab_or_collection)
    const {
  int start_index;
  int block_size;

  if (std::holds_alternative<tabs::TabInterface*>(tab_or_collection)) {
    if (count() == 1) {
      return std::nullopt;
    }

    tabs::TabInterface* tab = std::get<tabs::TabInterface*>(tab_or_collection);
    start_index = GetIndexOfTab(tab);
    block_size = 1;
  } else {
    tabs::TabCollection* collection =
        std::get<tabs::TabCollection*>(tab_or_collection);

    if (count() == static_cast<int>(collection->TabCountRecursive())) {
      return std::nullopt;
    }

    CHECK(collection && collection->TabCountRecursive() > 0);
    start_index = GetIndexOfTab(collection->GetTabAtIndexRecursive(0));
    block_size = collection->TabCountRecursive();
  }

  gfx::Range block_tabs = gfx::Range(start_index, start_index + block_size);

  // First preference is a tab the block opened.
  int new_selected_index = GetIndexOfNextWebContentsOpenedBy(block_tabs);
  if (new_selected_index != TabStripModel::kNoTab &&
      !IsTabCollapsed(new_selected_index)) {
    return GetTabIndexAfterClosing(new_selected_index, block_tabs);
  }

  // Second preference is a tab the block's opener opened.
  new_selected_index = GetIndexOfNextWebContentsOpenedByOpenerOf(block_tabs);

  if (new_selected_index != TabStripModel::kNoTab &&
      !IsTabCollapsed(new_selected_index)) {
    return GetTabIndexAfterClosing(new_selected_index, block_tabs);
  }

  // Third preference is the block's opener.
  for (size_t i = block_tabs.start(); i < block_tabs.end(); ++i) {
    tabs::TabInterface* opener = GetTabModelAtIndex(i)->opener();
    std::optional<int> opener_index =
        opener ? std::make_optional(GetIndexOfTab(opener)) : std::nullopt;
    if (opener && !block_tabs.Contains(gfx::Range(opener_index.value())) &&
        !IsTabCollapsed(opener_index.value())) {
      return GetTabIndexAfterClosing(opener_index.value(), block_tabs);
    }
  }

  // Fourth preference is a tab that belongs in the same parent collection as
  // `tab_or_collection`.
  const tabs::TabCollection* parent_collection_detached_object = nullptr;
  if (std::holds_alternative<tabs::TabInterface*>(tab_or_collection)) {
    tabs::TabInterface* tab = std::get<tabs::TabInterface*>(tab_or_collection);
    parent_collection_detached_object = tab->GetParentCollection();
  } else {
    tabs::TabCollection* collection =
        std::get<tabs::TabCollection*>(tab_or_collection);
    parent_collection_detached_object = collection->GetParentCollection();
  }

  // Check if either the right of the block is present in
  // `parent_collection_range` or the left of the block.
  if (parent_collection_detached_object->type() ==
          tabs::TabCollection::Type::GROUP ||
      parent_collection_detached_object->type() ==
          tabs::TabCollection::Type::SPLIT) {
    const int first_tab_index = GetIndexOfTab(
        parent_collection_detached_object->GetTabAtIndexRecursive(0));
    const gfx::Range parent_collection_range =
        gfx::Range(first_tab_index,
                   first_tab_index +
                       parent_collection_detached_object->TabCountRecursive());

    if (parent_collection_range.end() != block_tabs.end()) {
      return GetTabIndexAfterClosing(start_index + block_size, block_tabs);
    }

    if (parent_collection_range.start() != block_tabs.start()) {
      return GetTabIndexAfterClosing(start_index - 1, block_tabs);
    }
  }

  // Try to pick an uncollapsed index.
  std::optional<int> next_available = GetNextExpandedActiveTab(block_tabs);
  if (next_available.has_value()) {
    return GetTabIndexAfterClosing(next_available.value(), block_tabs);
  }

  // Otherwise, prefer picking the tab after the last tab in the block.
  const int first_tab_in_block = static_cast<int>(block_tabs.start());
  const int last_tab_in_block = static_cast<int>(block_tabs.end() - 1);

  if (last_tab_in_block >= (count() - 1)) {
    return first_tab_in_block - 1;
  }

  return last_tab_in_block + 1 - block_tabs.length();
}

std::vector<std::pair<tabs::TabInterface*, int>>
TabStripModel::GetTabsAndIndicesInSplit(split_tabs::SplitTabId split_id) {
  std::vector<std::pair<tabs::TabInterface*, int>> split_tabs_with_indices;

  split_tabs::SplitTabData* split_data = GetSplitData(split_id);
  std::vector<tabs::TabInterface*> split_tabs = split_data->ListTabs();

  if (split_tabs.empty()) {
    return split_tabs_with_indices;
  }

  // All the tabs in a split should be contiguous. Instead of using
  // GetIndexOfTab multiple times, call it on the first tab, then increment by
  // one for each subsequent tab.
  for (size_t index = GetIndexOfTab(split_tabs[0]);
       tabs::TabInterface* split_tab : split_tabs) {
    split_tabs_with_indices.emplace_back(split_tab, index++);
  }

  return split_tabs_with_indices;
}

gfx::Range TabStripModel::GetIndexRangeOfSplit(
    split_tabs::SplitTabId split_id) const {
  split_tabs::SplitTabData* split_data = GetSplitData(split_id);
  return split_data->GetIndexRange();
}

void TabStripModel::NotifyForegroundTabsWillEnterBackground() {
  for (tabs::TabInterface* tab : GetForegroundTabs()) {
    if (tab->IsActivated()) {
      static_cast<tabs::TabModel*>(GetActiveTab())
          ->WillDeactivate(base::PassKey<TabStripModel>());
    }
    static_cast<tabs::TabModel*>(tab)->WillBecomeHidden(
        base::PassKey<TabStripModel>());
  }
}

TabStripModel::ScopedTabStripModalUIImpl::ScopedTabStripModalUIImpl(
    TabStripModel* model)
    : model_(model) {
  CHECK(!model_->showing_modal_ui_);
  model_->showing_modal_ui_ = true;
}

TabStripModel::ScopedTabStripModalUIImpl::~ScopedTabStripModalUIImpl() {
  model_->showing_modal_ui_ = false;
}
