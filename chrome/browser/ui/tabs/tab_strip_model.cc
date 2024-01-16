// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
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
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_notes/user_notes_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

TabGroupModelFactory* factory_instance = nullptr;

class RenderWidgetHostVisibilityTracker;

// Works similarly to base::AutoReset but checks for access from the wrong
// thread as well as ensuring that the previous value of the re-entrancy guard
// variable was false.
class ReentrancyCheck {
 public:
  explicit ReentrancyCheck(bool* guard_flag) : guard_flag_(guard_flag) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!*guard_flag_);
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

// Installs RenderWidgetVisibilityTracker when the active tab has changed.
std::unique_ptr<RenderWidgetHostVisibilityTracker>
InstallRenderWidgetVisibilityTracker(const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return nullptr;

  content::RenderWidgetHost* track_host = nullptr;
  if (selection.new_contents &&
      selection.new_contents->GetRenderWidgetHostView()) {
    track_host = selection.new_contents->GetRenderWidgetHostView()
                     ->GetRenderWidgetHost();
  }
  return std::make_unique<RenderWidgetHostVisibilityTracker>(track_host);
}

// This tracks (and reports via UMA and tracing) how long it takes before a
// RenderWidgetHost is requested to become visible.
class RenderWidgetHostVisibilityTracker final
    : public content::RenderWidgetHostObserver {
 public:
  explicit RenderWidgetHostVisibilityTracker(content::RenderWidgetHost* host) {
    if (!host || host->GetView()->IsShowing())
      return;
    observation_.Observe(host);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ui,latency",
                                      "TabSwitchVisibilityRequest", this,
                                      "render_widget_host", host);
  }
  ~RenderWidgetHostVisibilityTracker() override = default;
  RenderWidgetHostVisibilityTracker(const RenderWidgetHostVisibilityTracker&) =
      delete;
  RenderWidgetHostVisibilityTracker& operator=(
      const RenderWidgetHostVisibilityTracker&) = delete;

 private:
  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* host,
                                         bool became_visible) override {
    // This used to be a DCHECK but there are cases where the tab preview
    // capture system is just finishing up a capture where we could get a
    // signal with `became_visible` being false. Therefore simple make sure
    // the render widget host actually became visible.
    if (became_visible) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.SelectionToVisibilityRequestTime", timer_.Elapsed(),
          base::Microseconds(1), base::Seconds(3), 50);
      TRACE_EVENT_NESTABLE_ASYNC_END0("ui,latency",
                                      "TabSwitchVisibilityRequest", this);
      observation_.Reset();
    }
  }

  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override {
    DCHECK(observation_.IsObservingSource(host));
    observation_.Reset();
  }

  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      observation_{this};
  base::ElapsedTimer timer_;
};

}  // namespace

TabGroupModelFactory::TabGroupModelFactory() {
  DCHECK(!factory_instance);
  factory_instance = this;
}

// static
TabGroupModelFactory* TabGroupModelFactory::GetInstance() {
  if (!factory_instance)
    factory_instance = new TabGroupModelFactory();
  return factory_instance;
}

std::unique_ptr<TabGroupModel> TabGroupModelFactory::Create(
    TabGroupController* controller) {
  return std::make_unique<TabGroupModel>(controller);
}

DetachedWebContents::DetachedWebContents(
    int index_before_any_removals,
    int index_at_time_of_removal,
    std::unique_ptr<WebContents> owned_contents,
    content::WebContents* contents,
    TabStripModelChange::RemoveReason remove_reason,
    absl::optional<SessionID> id)
    : owned_contents(std::move(owned_contents)),
      contents(contents),
      index_before_any_removals(index_before_any_removals),
      index_at_time_of_removal(index_at_time_of_removal),
      remove_reason(remove_reason),
      id(id) {}
DetachedWebContents::~DetachedWebContents() = default;
DetachedWebContents::DetachedWebContents(DetachedWebContents&&) = default;

// Holds all state necessary to send notifications for detached tabs.
struct TabStripModel::DetachNotifications {
  DetachNotifications(WebContents* initially_active_web_contents,
                      const ui::ListSelectionModel& selection_model)
      : initially_active_web_contents(initially_active_web_contents),
        selection_model(selection_model) {}
  DetachNotifications(const DetachNotifications&) = delete;
  DetachNotifications& operator=(const DetachNotifications&) = delete;
  ~DetachNotifications() = default;

  // The WebContents that was active prior to any detaches happening. If this
  // is nullptr, the active WebContents was not removed.
  //
  // It's safe to use a raw pointer here because the active web contents, if
  // detached, is owned by |detached_web_contents|.
  //
  // Once the notification for change of active web contents has been sent,
  // this field is set to nullptr.
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged>
      initially_active_web_contents = nullptr;

  // The WebContents that were recently detached. Observers need to be notified
  // about these. These must be updated after construction.
  std::vector<std::unique_ptr<DetachedWebContents>> detached_web_contents;

  // The selection model prior to any tabs being detached.
  const ui::ListSelectionModel selection_model;
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

constexpr int TabStripModel::kNoTab;

TabStripModel::TabStripModel(TabStripModelDelegate* delegate,
                             Profile* profile,
                             TabGroupModelFactory* group_model_factory)
    : delegate_(delegate), profile_(profile) {
  DCHECK(delegate_);

  if (group_model_factory)
    group_model_ = group_model_factory->Create(this);
  scrubbing_metrics_.Init();
}

TabStripModel::~TabStripModel() {
  for (auto& observer : observers_)
    observer.ModelDestroyed(TabStripModelObserver::ModelPasskey(), this);

  contents_data_.clear();
}

void TabStripModel::SetTabStripUI(TabStripModelObserver* observer) {
  DCHECK(!tab_strip_ui_was_set_);

  std::vector<TabStripModelObserver*> new_observers{observer};
  for (auto& old_observer : observers_)
    new_observers.push_back(&old_observer);

  observers_.Clear();

  for (auto* new_observer : new_observers)
    observers_.AddObserver(new_observer);

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

int TabStripModel::GetIndexOfTab(TabHandle tab_handle) const {
  const TabModel* tab_model = tab_handle.Get();
  if (tab_model == nullptr) {
    return kNoTab;
  }

  const auto is_same_tab = [tab_model](const std::unique_ptr<TabModel>& other) {
    return other.get() == tab_model;
  };

  const auto iter =
      std::find_if(contents_data_.cbegin(), contents_data_.cend(), is_same_tab);
  if (iter == contents_data_.cend()) {
    return kNoTab;
  }
  return iter - contents_data_.begin();
}

TabHandle TabStripModel::GetTabHandleAt(int index) const {
  CHECK(ContainsIndex(index));

  return contents_data_[index]->GetHandle();
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

int TabStripModel::InsertWebContentsAt(
    int index,
    std::unique_ptr<WebContents> contents,
    int add_types,
    absl::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  return InsertWebContentsAtImpl(index, std::move(contents), add_types, group);
}

std::unique_ptr<content::WebContents> TabStripModel::ReplaceWebContentsAt(
    int index,
    std::unique_ptr<WebContents> new_contents) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  delegate()->WillAddWebContents(new_contents.get());

  CHECK(ContainsIndex(index));

  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);
  WebContents* raw_new_contents = new_contents.get();
  std::unique_ptr<WebContents> old_contents =
      contents_data_[index]->ReplaceContents(std::move(new_contents));

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    selection.new_contents = raw_new_contents;
    selection.reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  }

  TabStripModelChange::Replace replace;
  replace.old_contents = old_contents.get();
  replace.new_contents = raw_new_contents;
  replace.index = index;
  TabStripModelChange change(replace);
  OnChange(change, selection);

  return old_contents;
}

std::unique_ptr<content::WebContents>
TabStripModel::DetachWebContentsAtForInsertion(int index) {
  auto dwc = DetachWebContentsWithReasonAt(
      index, TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip);
  return std::move(dwc->owned_contents);
}

void TabStripModel::DetachAndDeleteWebContentsAt(int index) {
  // Drops the returned unique pointer.
  DetachWebContentsWithReasonAt(index,
                                TabStripModelChange::RemoveReason::kDeleted);
}

std::unique_ptr<DetachedWebContents>
TabStripModel::DetachWebContentsWithReasonAt(
    int index,
    TabStripModelChange::RemoveReason reason) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(active_index(), kNoTab) << "Activate the TabStripModel by "
                                      "selecting at least one tab before "
                                      "trying to detach web contents.";
  WebContents* initially_active_web_contents =
      GetWebContentsAtImpl(active_index());

  DetachNotifications notifications(initially_active_web_contents,
                                    selection_model_);
  auto dwc = DetachWebContentsImpl(index, index,
                                   /*create_historical_tab=*/false, reason);
  notifications.detached_web_contents.push_back(std::move(dwc));
  SendDetachWebContentsNotifications(&notifications);
  return std::move(notifications.detached_web_contents[0]);
}

void TabStripModel::OnChange(const TabStripModelChange& change,
                             const TabStripSelectionChange& selection) {
  OnActiveTabChanged(selection);

  for (auto& observer : observers_) {
    observer.OnTabStripModelChanged(this, change, selection);
  }
}

std::unique_ptr<DetachedWebContents> TabStripModel::DetachWebContentsImpl(
    int index_before_any_removals,
    int index_at_time_of_removal,
    bool create_historical_tab,
    TabStripModelChange::RemoveReason reason) {
  if (contents_data_.empty())
    return nullptr;
  CHECK(ContainsIndex(index_at_time_of_removal));

  for (auto& observer : observers_) {
    observer.OnTabWillBeRemoved(
        contents_data_[index_at_time_of_removal]->contents(),
        index_at_time_of_removal);
  }

  FixOpeners(index_at_time_of_removal);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database.
  WebContents* raw_web_contents =
      GetWebContentsAtImpl(index_at_time_of_removal);
  absl::optional<SessionID> id = absl::nullopt;
  if (create_historical_tab)
    id = delegate_->CreateHistoricalTab(raw_web_contents);

  absl::optional<int> next_selected_index =
      DetermineNewSelectedIndex(index_at_time_of_removal);

  UngroupTab(index_at_time_of_removal);

  std::unique_ptr<TabModel> old_data =
      std::move(contents_data_[index_at_time_of_removal]);
  contents_data_.erase(contents_data_.begin() + index_at_time_of_removal);

  if (empty()) {
    selection_model_.Clear();
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index_at_time_of_removal);
    ui::ListSelectionModel old_model;
    old_model = selection_model_;
    if (index_at_time_of_removal == old_active) {
      if (!selection_model_.empty()) {
        // The active tab was removed, but there is still something selected.
        // Move the active and anchor to the first selected index.
        selection_model_.set_active(
            *selection_model_.selected_indices().begin());
        selection_model_.set_anchor(selection_model_.active());
      } else {
        DCHECK(next_selected_index.has_value());
        // The active tab was removed and nothing is selected. Reset the
        // selection and send out notification.
        selection_model_.SetSelectedIndex(next_selected_index.value());
      }
    }
  }

  auto owned_contents = old_data->ReplaceContents(nullptr);
  auto* contents = owned_contents.get();
  return std::make_unique<DetachedWebContents>(
      index_before_any_removals, index_at_time_of_removal,
      std::move(owned_contents), contents, reason, id);
}

void TabStripModel::SendDetachWebContentsNotifications(
    DetachNotifications* notifications) {
  // Sort the DetachedWebContents in decreasing order of
  // |index_before_any_removals|. This is because |index_before_any_removals| is
  // used by observers to update their own copy of TabStripModel state, and each
  // removal affects subsequent removals of higher index.
  std::sort(notifications->detached_web_contents.begin(),
            notifications->detached_web_contents.end(),
            [](const std::unique_ptr<DetachedWebContents>& dwc1,
               const std::unique_ptr<DetachedWebContents>& dwc2) {
              return dwc1->index_before_any_removals >
                     dwc2->index_before_any_removals;
            });

  TabStripModelChange::Remove remove;
  for (auto& dwc : notifications->detached_web_contents) {
    remove.contents.emplace_back(dwc->contents, dwc->index_before_any_removals,
                                 dwc->remove_reason, dwc->id);
  }
  TabStripModelChange change(std::move(remove));

  TabStripSelectionChange selection;
  selection.old_contents = notifications->initially_active_web_contents;
  selection.new_contents = GetActiveWebContents();
  selection.old_model = notifications->selection_model;
  selection.new_model = selection_model_;
  selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
  selection.selected_tabs_were_removed = base::ranges::any_of(
      notifications->detached_web_contents, [&notifications](auto& dwc) {
        return notifications->selection_model.IsSelected(
            dwc->index_before_any_removals);
      });
  {
    auto visibility_tracker =
        empty() ? nullptr : InstallRenderWidgetVisibilityTracker(selection);

    OnChange(change, selection);
  }

  for (auto& dwc : notifications->detached_web_contents) {
    if (dwc->remove_reason == TabStripModelChange::RemoveReason::kDeleted) {
      // This destroys the WebContents, which will also send
      // WebContentsDestroyed notifications.
      dwc->owned_contents.reset();
      dwc->contents = nullptr;
    }
  }

  if (empty()) {
    for (auto& observer : observers_)
      observer.TabStripEmpty();
  }
}

void TabStripModel::ActivateTabAt(int index,
                                  TabStripUserGestureDetails user_gesture) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));
  TRACE_EVENT0("ui", "TabStripModel::ActivateTabAt");

  scrubbing_metrics_.IncrementPressCount(user_gesture);

  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
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

  to_position = ConstrainMoveIndex(to_position, IsTabPinned(index));

  if (index == to_position)
    return to_position;

  MoveWebContentsAtImpl(index, to_position, select_after_move);
  EnsureGroupContiguity(to_position);

  return to_position;
}

void TabStripModel::MoveSelectedTabsTo(int index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  int total_pinned_count = IndexOfFirstNonPinnedTab();
  int selected_pinned_count = 0;
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model_.selected_indices();
  int selected_count = static_cast<int>(selected_indices.size());
  for (auto selection : selected_indices) {
    if (IsTabPinned(selection))
      selected_pinned_count++;
  }

  // To maintain that all pinned tabs occur before non-pinned tabs we move them
  // first.
  if (selected_pinned_count > 0) {
    MoveSelectedTabsToImpl(
        std::min(total_pinned_count - selected_pinned_count, index), 0u,
        selected_pinned_count);
    if (index > total_pinned_count - selected_pinned_count) {
      // We're being told to drag pinned tabs to an invalid location. Adjust the
      // index such that non-pinned tabs end up at a location as though we could
      // move the pinned tabs to index. See description in header for more
      // details.
      index += selected_pinned_count;
    }
  }
  if (selected_pinned_count == selected_count)
    return;

  // Then move the non-pinned tabs.
  MoveSelectedTabsToImpl(std::max(index, total_pinned_count),
                         selected_pinned_count,
                         selected_count - selected_pinned_count);
}

void TabStripModel::MoveGroupTo(const tab_groups::TabGroupId& group,
                                int to_index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(to_index, kNoTab);
  to_index = ConstrainMoveIndex(to_index, false /* pinned tab */);

  if (!group_model_)
    return;

  gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  DCHECK_GT(tabs_in_group.length(), 0u);

  int from_index = tabs_in_group.start();
  if (to_index < from_index)
    from_index = tabs_in_group.end() - 1;

  for (size_t i = 0; i < tabs_in_group.length(); ++i)
    MoveWebContentsAtImpl(from_index, to_index, false);

  MoveTabGroup(group);
}

WebContents* TabStripModel::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  if (ContainsIndex(index))
    return GetWebContentsAtImpl(index);
  return nullptr;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (contents_data_[i]->contents() == contents) {
      return i;
    }
  }
  return kNoTab;
}

void TabStripModel::UpdateWebContentsStateAt(int index,
                                             TabChangeType change_type) {
  WebContents* const web_contents = GetWebContentsAtImpl(index);

  for (auto& observer : observers_) {
    observer.TabChangedAt(web_contents, index, change_type);
  }
}

void TabStripModel::SetTabNeedsAttentionAt(int index, bool attention) {
  CHECK(ContainsIndex(index));

  for (auto& observer : observers_)
    observer.SetTabNeedsAttentionAt(index, attention);
}

void TabStripModel::CloseAllTabs() {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(count());
  for (int i = count() - 1; i >= 0; --i)
    closing_tabs.push_back(GetWebContentsAt(i));
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseAllTabsInGroup(const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_)
    return;

  delegate_->CreateHistoricalGroup(group);

  gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  if (static_cast<int>(tabs_in_group.length()) == count())
    closing_all_ = true;

  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(tabs_in_group.length());
  for (uint32_t i = tabs_in_group.end(); i > tabs_in_group.start(); --i)
    closing_tabs.push_back(GetWebContentsAt(i - 1));
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  CHECK(ContainsIndex(index));
  WebContents* contents = GetWebContentsAt(index);
  CloseTabs(base::span<WebContents* const>(&contents, 1u), close_types);
}

bool TabStripModel::TabsAreLoading() const {
  for (const auto& data : contents_data_) {
    if (data->contents()->IsLoading()) {
      return true;
    }
  }

  return false;
}

WebContents* TabStripModel::GetOpenerOfWebContentsAt(const int index) const {
  CHECK(ContainsIndex(index));
  return contents_data_[index]->opener();
}

void TabStripModel::SetOpenerOfWebContentsAt(int index, WebContents* opener) {
  CHECK(ContainsIndex(index));
  // The TabStripModel only maintains the references to openers that it itself
  // owns; trying to set an opener to an external WebContents can result in
  // the opener being used after its freed. See crbug.com/698681.
  DCHECK(!opener || GetIndexOfWebContents(opener) != kNoTab)
      << "Cannot set opener to a web contents not owned by this tab strip.";
  contents_data_[index]->set_opener(opener);
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  CHECK(ContainsIndex(start_index));

  std::set<const WebContents*> opener_and_descendants;
  opener_and_descendants.insert(opener);
  int last_index = kNoTab;

  for (int i = start_index + 1; i < count(); ++i) {
    // Test opened by transitively, i.e. include tabs opened by tabs opened by
    // opener, etc. Stop when we find the first non-descendant.
    if (!opener_and_descendants.count(contents_data_[i]->opener())) {
      // Skip over pinned tabs as new tabs are added after pinned tabs.
      if (contents_data_[i]->pinned())
        continue;
      break;
    }
    opener_and_descendants.insert(contents_data_[i]->contents());
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
  if (contents_data_[index]->blocked() == blocked)
    return;
  contents_data_[index]->set_blocked(blocked);
  for (auto& observer : observers_)
    observer.TabBlockedStateChanged(contents_data_[index]->contents(), index);
}

int TabStripModel::SetTabPinned(int index, bool pinned) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  return SetTabPinnedImpl(index, pinned);
}

bool TabStripModel::IsTabPinned(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return contents_data_[index]->pinned();
}

bool TabStripModel::IsTabCollapsed(int index) const {
  absl::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  return group.has_value() && IsGroupCollapsed(group.value());
}

bool TabStripModel::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  DCHECK(group_model_);

  return group_model()->ContainsTabGroup(group) &&
         group_model()->GetTabGroup(group)->visual_data()->is_collapsed();
}

bool TabStripModel::IsTabBlocked(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return contents_data_[index]->blocked();
}

bool TabStripModel::IsTabClosable(int index) const {
  return PolicyAllowsTabClosing(GetWebContentsAt(index));
}

bool TabStripModel::IsTabClosable(const content::WebContents* contents) const {
  return IsTabClosable(GetIndexOfWebContents(contents));
}

absl::optional<tab_groups::TabGroupId> TabStripModel::GetTabGroupForTab(
    int index) const {
  return ContainsIndex(index) ? contents_data_[index]->group() : absl::nullopt;
}

absl::optional<tab_groups::TabGroupId> TabStripModel::GetSurroundingTabGroup(
    int index) const {
  if (!ContainsIndex(index - 1) || !ContainsIndex(index))
    return absl::nullopt;

  // If the tab before is not in a group, a tab inserted at |index|
  // wouldn't be surrounded by one group.
  absl::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index - 1);
  if (!group)
    return absl::nullopt;

  // If the tab after is in a different (or no) group, a new tab at
  // |index| isn't surrounded.
  if (group != GetTabGroupForTab(index))
    return absl::nullopt;
  return group;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!IsTabPinned(static_cast<int>(i)))
      return static_cast<int>(i);
  }
  // No pinned tabs.
  return count();
}

void TabStripModel::ExtendSelectionTo(int index) {
  CHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::ToggleSelectionAt(int index) {
  if (!delegate()->IsTabStripEditable())
    return false;
  CHECK(ContainsIndex(index));
  const size_t index_size_t = static_cast<size_t>(index);
  ui::ListSelectionModel new_model = selection_model();
  if (selection_model_.IsSelected(index_size_t)) {
    if (selection_model_.size() == 1) {
      // One tab must be selected and this tab is currently selected so we can't
      // unselect it.
      return false;
    }
    new_model.RemoveIndexFromSelection(index_size_t);
    new_model.set_anchor(index_size_t);
    if (!new_model.active().has_value() || new_model.active() == index_size_t)
      new_model.set_active(*new_model.selected_indices().begin());
  } else {
    new_model.AddIndexToSelection(index_size_t);
    new_model.set_anchor(index_size_t);
    new_model.set_active(index_size_t);
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
  return true;
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model = selection_model_;
  new_model.AddSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::IsTabSelected(int index) const {
  CHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

absl::optional<base::Time> TabStripModel::GetLastAccessed(int index) const {
  if (ContainsIndex(index)) {
    return selection_model_.GetLastAccessed(index);
  }
  return absl::nullopt;
}

void TabStripModel::SetSelectionFromModel(ui::ListSelectionModel source) {
  CHECK(source.active().has_value());
  SetSelection(std::move(source), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

const ui::ListSelectionModel& TabStripModel::selection_model() const {
  return selection_model_;
}

void TabStripModel::AddWebContents(
    std::unique_ptr<WebContents> contents,
    int index,
    ui::PageTransition transition,
    int add_types,
    absl::optional<tab_groups::TabGroupId> group) {
  for (auto& observer : observers_)
    observer.OnTabWillBeAdded();

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
    if (!group.has_value())
      group = GetTabGroupForTab(active_index());
  } else {
    // For all other types, respect what was passed to us, normalizing -1s and
    // values that are too large.
    if (index < 0 || index > count())
      index = count();
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
      group = absl::nullopt;
    }
  } else {
    group = absl::nullopt;
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
  WebContents* raw_contents = contents.get();
  InsertWebContentsAtImpl(index, std::move(contents),
                          add_types | (inherit_opener ? ADD_INHERIT_OPENER : 0),
                          group);
  // Reset the index, just in case insert ended up moving it on us.
  index = GetIndexOfWebContents(raw_contents);

  // In the "quick look-up" case detailed above, we want to reset the opener
  // relationship on any active tab change, even to another tab in the same tree
  // of openers. A jump would be too confusing at that point.
  if (inherit_opener && ui::PageTransitionTypeIncludingQualifiersIs(
                            transition, ui::PAGE_TRANSITION_TYPED))
    contents_data_[index]->set_reset_opener_on_active_tab_change(true);

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
      raw_contents->Resize(
          gfx::Rect(old_contents->GetContainerBounds().size()));
    }
  }
}

void TabStripModel::CloseSelectedTabs() {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  const ui::ListSelectionModel::SelectedIndices& sel =
      selection_model_.selected_indices();
  CloseTabs(GetWebContentsesByIndices(std::vector<int>(sel.begin(), sel.end())),
            TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                TabCloseTypes::CLOSE_USER_GESTURE);
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

tab_groups::TabGroupId TabStripModel::AddToNewGroup(
    const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(SupportsTabGroups());

  // Ensure that the indices are nonempty, sorted, and unique.
  DCHECK_GT(indices.size(), 0u);
  DCHECK(base::ranges::is_sorted(indices));
  DCHECK(base::ranges::adjacent_find(indices) == indices.end());

  // The odds of |new_group| colliding with an existing group are astronomically
  // low. If there is a collision, a DCHECK will fail in |AddToNewGroupImpl()|,
  // in which case there is probably something wrong with
  // |tab_groups::TabGroupId::GenerateNew()|.
  const tab_groups::TabGroupId new_group =
      tab_groups::TabGroupId::GenerateNew();
  AddToNewGroupImpl(indices, new_group);
  return new_group;
}

void TabStripModel::AddToExistingGroup(const std::vector<int>& indices,
                                       const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(SupportsTabGroups());

  // Ensure that the indices are sorted and unique.
  DCHECK(base::ranges::is_sorted(indices));
  DCHECK(base::ranges::adjacent_find(indices) == indices.end());
  CHECK(ContainsIndex(*(indices.begin())));
  CHECK(ContainsIndex(*(indices.rbegin())));

  AddToExistingGroupImpl(indices, group);
}

void TabStripModel::MoveTabsAndSetGroup(
    const std::vector<int>& indices,
    int destination_index,
    absl::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_)
    return;

  MoveTabsAndSetGroupImpl(indices, destination_index, group);
}

void TabStripModel::AddToGroupForRestore(const std::vector<int>& indices,
                                         const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_)
    return;

  const bool group_exists = group_model_->ContainsTabGroup(group);
  if (group_exists)
    AddToExistingGroupImpl(indices, group);
  else
    AddToNewGroupImpl(indices, group);
}

void TabStripModel::UpdateGroupForDragRevert(
    int index,
    absl::optional<tab_groups::TabGroupId> group_id,
    absl::optional<tab_groups::TabGroupVisualData> group_data) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_)
    return;

  if (group_id.has_value()) {
    const bool group_exists = group_model_->ContainsTabGroup(group_id.value());
    if (!group_exists)
      group_model_->AddTabGroup(group_id.value(), group_data);
    GroupTab(index, group_id.value());
  } else {
    UngroupTab(index);
  }
}

void TabStripModel::RemoveFromGroup(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_)
    return;

  std::map<tab_groups::TabGroupId, std::vector<int>> indices_per_tab_group;

  for (int index : indices) {
    absl::optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
    if (old_group.has_value())
      indices_per_tab_group[old_group.value()].push_back(index);
  }

  for (const auto& kv : indices_per_tab_group) {
    const TabGroup* group = group_model_->GetTabGroup(kv.first);
    const int first_tab_in_group = group->GetFirstTab().value();
    const int last_tab_in_group = group->GetLastTab().value();

    // This is an estimate. If the group is non-contiguous it will be
    // larger than the true size. This can happen while dragging tabs in
    // or out of a group.
    const int num_tabs_in_group = last_tab_in_group - first_tab_in_group + 1;
    const int group_midpoint = first_tab_in_group + num_tabs_in_group / 2;

    // Split group into |left_of_group| and |right_of_group| depending on
    // whether the index is closest to the left or right edge.
    std::vector<int> left_of_group;
    std::vector<int> right_of_group;
    for (int index : kv.second) {
      if (index < group_midpoint) {
        left_of_group.push_back(index);
      } else {
        right_of_group.push_back(index);
      }
    }
    MoveTabsAndSetGroupImpl(left_of_group, first_tab_in_group, absl::nullopt);
    MoveTabsAndSetGroupImpl(right_of_group, last_tab_in_group + 1,
                            absl::nullopt);
  }
}

bool TabStripModel::IsReadLaterSupportedForAny(
    const std::vector<int>& indices) {
  if (!delegate_->SupportsReadLater())
    return false;

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  if (!model || !model->loaded())
    return false;
  for (int index : indices) {
    if (model->IsUrlSupported(
            chrome::GetURLToBookmark(GetWebContentsAt(index))))
      return true;
  }
  return false;
}

void TabStripModel::AddToReadLater(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  AddToReadLaterImpl(indices);
}

void TabStripModel::CreateTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, TabGroupChange::kCreated);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::OpenTabGroupEditor(const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, TabGroupChange::kEditorOpened);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::ChangeTabGroupContents(
    const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, TabGroupChange::kContentsChanged);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::ChangeTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const TabGroupChange::VisualsChange& visuals) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, visuals);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::MoveTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, TabGroupChange::kMoved);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::CloseTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  TabGroupChange change(this, group, TabGroupChange::kClosed);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

std::u16string TabStripModel::GetTitleAt(int index) const {
  return TabUIHelper::FromWebContents(GetWebContentsAt(index))->GetTitle();
}

void TabStripModel::FollowSites(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  for (int index : indices)
    delegate_->FollowSite(GetWebContentsAt(index));
}

void TabStripModel::UnfollowSites(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  for (int index : indices)
    delegate_->UnfollowSite(GetWebContentsAt(index));
}

int TabStripModel::GetTabCount() const {
  return static_cast<int>(contents_data_.size());
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTabToRight:
    case CommandCloseTab:
      return true;

    case CommandReload:
      return delegate_->CanReload();

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight:
      return !GetIndicesClosedByCommand(context_index, command_id).empty();

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
        if (!GetWebContentsAt(index)->GetLastCommittedURL().is_empty())
          return true;
      }
      return false;
    }

    case CommandTogglePinned:
      return true;

    case CommandToggleGrouped:
      return SupportsTabGroups();

    case CommandSendTabToSelf:
      return true;

    case CommandAddNote: {
      DCHECK(UserNotesController::IsUserNotesSupported(profile()));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      if (indices.size() != 1) {
        return false;
      }
      content::WebContents* web_contents = GetWebContentsAt(indices[0]);
      return UserNotesController::IsUserNotesSupported(web_contents);
    }

    case CommandAddToReadLater:
      return true;

    case CommandAddToNewGroup:
      return SupportsTabGroups();

    case CommandAddToExistingGroup:
      return SupportsTabGroups();

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

    case CommandFollowSite:
    case CommandUnfollowSite: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Since all tabs should belong to same profile, it is enough to do the
      // check based on the first tab.
      content::WebContents* web_contents = GetWebContentsAt(indices[0]);
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      return !profile->IsIncognitoProfile();
    }

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
  return false;
}

void TabStripModel::ExecuteContextMenuCommand(int context_index,
                                              ContextMenuCommand command_id) {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!ContainsIndex(context_index))
    return;
  switch (command_id) {
    case CommandNewTabToRight: {
      base::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", NewTabTypes::NEW_TAB_CONTEXT_MENU,
                                NewTabTypes::NEW_TAB_ENUM_COUNT);
      delegate()->AddTabAt(GURL(), context_index + 1, true,
                           GetTabGroupForTab(context_index));
      break;
    }

    case CommandReload: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      if (!delegate_->CanReload())
        break;
      for (int index : GetIndicesForCommand(context_index)) {
        WebContents* tab = GetWebContentsAt(index);
        if (tab)
          tab->GetController().Reload(content::ReloadType::NORMAL, true);
      }
      break;
    }

    case CommandDuplicate: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the WebContents off as the indices will change as tabs are
      // duplicated.
      std::vector<WebContents*> tabs;
      for (int index : indices) {
        tabs.push_back(GetWebContentsAt(index));
      }
      for (const WebContents* const tab : tabs) {
        int index = GetIndexOfWebContents(tab);
        if (index != -1 && delegate()->CanDuplicateContentsAt(index))
          delegate()->DuplicateContentsAt(index);
      }
      break;
    }

    case CommandCloseTab: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      CloseTabs(GetWebContentsesByIndices(GetIndicesForCommand(context_index)),
                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                    TabCloseTypes::CLOSE_USER_GESTURE);
      break;
    }

    case CommandCloseOtherTabs: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      const std::vector<int> indices =
          GetIndicesClosedByCommand(context_index, command_id);

      DisconnectSavedTabGroups(indices);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      CloseTabs(GetWebContentsesByIndices(indices),
                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandCloseTabsToRight: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      const std::vector<int> indices =
          GetIndicesClosedByCommand(context_index, command_id);

      DisconnectSavedTabGroups(indices);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      CloseTabs(GetWebContentsesByIndices(indices),
                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandSendTabToSelf: {
      send_tab_to_self::ShowBubble(GetWebContentsAt(context_index));
      break;
    }

    case CommandTogglePinned: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool pin = WillContextMenuPin(context_index);
      SetTabsPinned(indices, pin);
      break;
    }

    case CommandToggleGrouped: {
      if (!group_model_)
        break;

      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool group = WillContextMenuGroup(context_index);
      if (group) {
        absl::optional<tab_groups::TabGroupId> new_group_id =
            AddToNewGroup(indices);
        if (new_group_id.has_value())
          OpenTabGroupEditor(new_group_id.value());
      } else {
        RemoveFromGroup(indices);
      }

      break;
    }

    case CommandToggleSiteMuted: {
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

    case CommandAddNote: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      DCHECK(indices.size() == 1);
      Browser* browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(indices.front()));
      UserNotesController::InitiateNoteCreationForTab(browser, indices.front());
      break;
    }

    case CommandAddToReadLater: {
      base::RecordAction(
          UserMetricsAction("DesktopReadingList.AddItem.FromTabContextMenu"));
      AddToReadLater(GetIndicesForCommand(context_index));
      break;
    }

    case CommandAddToNewGroup: {
      if (!group_model_)
        break;

      base::RecordAction(UserMetricsAction("TabContextMenu_AddToNewGroup"));

      absl::optional<tab_groups::TabGroupId> new_group_id =
          AddToNewGroup(GetIndicesForCommand(context_index));
      OpenTabGroupEditor(new_group_id.value());
      break;
    }

    case CommandAddToExistingGroup: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingGroupCommand with the correct group later.
      break;
    }

    case CommandRemoveFromGroup: {
      if (!group_model_)
        break;

      base::RecordAction(UserMetricsAction("TabContextMenu_RemoveFromGroup"));
      RemoveFromGroup(GetIndicesForCommand(context_index));
      break;
    }

    case CommandMoveToExistingWindow: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingWindowCommand with the correct window later.
      break;
    }

    case CommandMoveTabsToNewWindow: {
      base::RecordAction(
          UserMetricsAction("TabContextMenu_MoveTabToNewWindow"));
      delegate()->MoveTabsToNewWindow(GetIndicesForCommand(context_index));
      break;
    }

    case CommandOrganizeTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_OrganizeTabs"));
      const Browser* const browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(context_index));
      TabOrganizationService* const service =
          TabOrganizationServiceFactory::GetForProfile(profile_);
      CHECK(service);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.TabContextMenu.Clicked", true);

      service->RestartSessionAndShowUI(browser,
                                       GetWebContentsAt(context_index));
      break;
    }

    case CommandFollowSite: {
      base::RecordAction(UserMetricsAction("DesktopFeed.FollowSite"));
      FollowSites(GetIndicesForCommand(context_index));
      break;
    }

    case CommandUnfollowSite: {
      base::RecordAction(UserMetricsAction("DesktopFeed.UnfollowSite"));
      UnfollowSites(GetIndicesForCommand(context_index));
      break;
    }

    case CommandCopyURL: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CopyURL"));
      delegate()->CopyURL(GetWebContentsAt(context_index));
      break;
    }

    case CommandGoBack: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Back"));
      delegate()->GoBack(GetWebContentsAt(context_index));
      break;
    }

    case CommandCloseAllTabs: {
      // Closes all tabs except the pinned home tab.
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseAllTabs"));

      std::vector<int> indices;
      for (int i = count() - 1; i > 0; --i) {
        indices.push_back(i);
      }

      CloseTabs(GetWebContentsesByIndices(indices),
                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);

      break;
    }

    default:
      NOTREACHED();
  }
}

void TabStripModel::ExecuteAddToExistingGroupCommand(
    int context_index,
    const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingGroup"));

  if (!ContainsIndex(context_index))
    return;
  AddToExistingGroup(GetIndicesForCommand(context_index), group);
}

void TabStripModel::ExecuteAddToExistingWindowCommand(int context_index,
                                                      int browser_index) {
  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingWindow"));

  if (!ContainsIndex(context_index))
    return;
  delegate()->MoveToExistingWindow(GetIndicesForCommand(context_index),
                                   browser_index);
}

bool TabStripModel::WillContextMenuMuteSites(int index) {
  return !chrome::AreAllSitesMuted(*this, GetIndicesForCommand(index));
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i)
    all_pinned = IsTabPinned(indices[i]);
  return !all_pinned;
}

bool TabStripModel::WillContextMenuGroup(int index) {
  if (!group_model_)
    return false;

  std::vector<int> indices = GetIndicesForCommand(index);
  DCHECK(!indices.empty());

  // If all tabs are in the same group, then we ungroup, otherwise we group.
  absl::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(indices[0]);
  if (!group.has_value())
    return true;

  for (size_t i = 1; i < indices.size(); ++i) {
    if (GetTabGroupForTab(indices[i]) != group)
      return true;
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

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  CHECK(ContainsIndex(start_index));

  // Check tabs after start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    if (contents_data_[i]->opener() == opener)
      return i;
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    if (contents_data_[i]->opener() == opener)
      return i;
  }
  return kNoTab;
}

absl::optional<int> TabStripModel::GetNextExpandedActiveTab(
    int start_index,
    absl::optional<tab_groups::TabGroupId> collapsing_group) const {
  // Check tabs from the start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    absl::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    absl::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  return absl::nullopt;
}

void TabStripModel::ForgetAllOpeners() {
  for (const auto& data : contents_data_)
    data->set_opener(nullptr);
}

void TabStripModel::ForgetOpener(WebContents* contents) {
  const int index = GetIndexOfWebContents(contents);
  CHECK(ContainsIndex(index));
  contents_data_[index]->set_opener(nullptr);
}

void TabStripModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("active_index", active_index());
  dict.Add("tabs", contents_data_);
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
  if (!IsTabSelected(index))
    return {index};
  const ui::ListSelectionModel::SelectedIndices& sel =
      selection_model_.selected_indices();
  return std::vector<int>(sel.begin(), sel.end());
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  CHECK(ContainsIndex(index));
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int last_unclosed_tab = -1;
  if (id == CommandCloseTabsToRight) {
    last_unclosed_tab =
        is_selected ? *selection_model_.selected_indices().rbegin() : index;
  }

  // NOTE: callers expect the vector to be sorted in descending order.
  std::vector<int> indices;
  for (int i = count() - 1; i > last_unclosed_tab; --i) {
    if (i != index && !IsTabPinned(i) && (!is_selected || !IsTabSelected(i)))
      indices.push_back(i);
  }
  return indices;
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetLastCommittedURL();
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost &&
         contents == GetWebContentsAtImpl(count() - 1) &&
         contents->GetController().GetEntryCount() == 1;
}

std::vector<content::WebContents*> TabStripModel::GetWebContentsesByIndices(
    const std::vector<int>& indices) {
  std::vector<content::WebContents*> items;
  items.reserve(indices.size());
  for (int index : indices)
    items.push_back(GetWebContentsAtImpl(index));
  return items;
}

int TabStripModel::InsertWebContentsAtImpl(
    int index,
    std::unique_ptr<content::WebContents> contents,
    int add_types,
    absl::optional<tab_groups::TabGroupId> group) {
  delegate()->WillAddWebContents(contents.get());

  bool active = (add_types & ADD_ACTIVE) != 0;
  bool pin = (add_types & ADD_PINNED) != 0;
  index = ConstrainInsertionIndex(index, pin);

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  WebContents* active_contents = GetActiveWebContents();
  WebContents* raw_contents = contents.get();
  std::unique_ptr<TabModel> data =
      std::make_unique<TabModel>(std::move(contents));
  data->set_pinned(pin);
  if ((add_types & ADD_INHERIT_OPENER) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple openers active at the same time.
      ForgetAllOpeners();
    }
    data->set_opener(active_contents);
  }

  // TODO(gbillock): Ask the modal dialog manager whether the WebContents should
  // be blocked, or just let the modal dialog manager make the blocking call
  // directly and not use this at all.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(raw_contents);
  if (manager)
    data->set_blocked(manager->IsDialogActive());

  // Force the group value to be set since we perform contiguity checks on the
  // tab groups when rendered in views. this will not inform the observers of
  // the group change until GroupTab called after OnTabStripModelChanged.
  data->set_group(group);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);

  contents_data_.insert(contents_data_.begin() + index, std::move(data));

  selection_model_.IncrementFrom(index);

  if (active) {
    ui::ListSelectionModel new_model = selection_model_;
    new_model.SetSelectedIndex(index);
    selection = SetSelection(std::move(new_model),
                             TabStripModelObserver::CHANGE_REASON_NONE,
                             /*triggered_by_other_operation=*/true);
  }

  TabStripModelChange::Insert insert;
  insert.contents.push_back({raw_contents, index});
  TabStripModelChange change(std::move(insert));
  OnChange(change, selection);

  if (group_model_ && group.has_value()) {
    // Unset the group at the index of the inserted WebContents so that the
    // GroupTab functionality isn't skipped.
    contents_data_[index]->set_group(absl::nullopt);
    GroupTab(index, group.value());
  }

  return index;
}

void TabStripModel::CloseTabs(base::span<content::WebContents* const> items,
                              uint32_t close_types) {
  std::vector<content::WebContents*> filtered_items;
  base::ranges::copy_if(items, std::back_inserter(filtered_items),
                        [this](content::WebContents* const contents) {
                          return IsTabClosable(contents);
                        });

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

  DetachNotifications notifications(GetActiveWebContents(), selection_model_);
  const bool closed_all =
      CloseWebContentses(filtered_items, close_types, &notifications);

  // When unload handler is triggered for all items, we should wait for the
  // result.
  if (!notifications.detached_web_contents.empty())
    SendDetachWebContentsNotifications(&notifications);

  if (!ref)
    return;
  if (closing_all) {
    // CloseAllTabsStopped is sent with reason kCloseAllCompleted if
    // closed_all; otherwise kCloseAllCanceled is sent.
    for (auto& observer : observers_)
      observer.CloseAllTabsStopped(
          this, closed_all ? TabStripModelObserver::kCloseAllCompleted
                           : TabStripModelObserver::kCloseAllCanceled);
  }
}

bool TabStripModel::CloseWebContentses(
    base::span<content::WebContents* const> items,
    uint32_t close_types,
    DetachNotifications* notifications) {
  if (items.empty())
    return true;

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (!browser_shutdown::HasShutdownStarted()) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* contents : items) {
      if (ShouldRunUnloadListenerBeforeClosing(contents))
        continue;
      content::RenderProcessHost* process =
          contents->GetPrimaryMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes)
      pair.first->FastShutdownIfPossible(pair.second, false);
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool closed_all = true;

  // The indices of WebContents prior to any modification of the internal state.
  std::vector<int> original_indices;
  original_indices.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i)
    original_indices[i] = GetIndexOfWebContents(items[i]);

  std::vector<std::unique_ptr<DetachedWebContents>> detached_web_contents;
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
    auto dwc = DetachWebContentsImpl(
        original_indices[i], current_index, create_historical_tab,
        TabStripModelChange::RemoveReason::kDeleted);
    detached_web_contents.push_back(std::move(dwc));
  }

  if (!detached_web_contents.empty()) {
    // ClosedTabCache will only take ownership of the last recently closed tab.
    delegate_->CacheWebContents(detached_web_contents);
  }

  // If the delegate takes ownership, it must reset the reason.
#if DCHECK_IS_ON()
  for (auto& dwc : detached_web_contents)
    if (dwc->owned_contents) {
      DCHECK_EQ(TabStripModelChange::RemoveReason::kDeleted,
                dwc->remove_reason);
    } else {
      DCHECK_EQ(TabStripModelChange::RemoveReason::kCached, dwc->remove_reason);
    }
#endif

  for (auto& dwc : detached_web_contents)
    notifications->detached_web_contents.push_back(std::move(dwc));

  return closed_all;
}

WebContents* TabStripModel::GetWebContentsAtImpl(int index) const {
  CHECK(ContainsIndex(index))
      << "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_[index]->contents();
}

TabStripSelectionChange TabStripModel::SetSelection(
    ui::ListSelectionModel new_model,
    TabStripModelObserver::ChangeReason reason,
    bool triggered_by_other_operation) {
  TabStripSelectionChange selection;
  selection.old_model = selection_model_;
  selection.old_contents = GetActiveWebContents();
  selection.new_model = new_model;
  selection.reason = reason;

#if DCHECK_IS_ON()
  // Validate that |new_model| only selects tabs that actually exist.
  DCHECK(new_model.active().has_value());
  DCHECK(ContainsIndex(new_model.active().value()));
  for (size_t selected_index : new_model.selected_indices()) {
    DCHECK(ContainsIndex(selected_index));
  }
#endif

  // This is done after notifying TabDeactivated() because caller can assume
  // that TabStripModel::active_index() would return the index for
  // |selection.old_contents|.
  selection_model_ = new_model;
  selection.new_contents = GetActiveWebContents();

  if (!triggered_by_other_operation &&
      (selection.active_tab_changed() || selection.selection_changed())) {
    if (selection.active_tab_changed()) {
      // Start measuring the tab switch compositing time. This must be the first
      // thing in this block so that the start time is saved before any changes
      // that might affect compositing.
      if (selection.new_contents) {
        selection.new_contents->SetTabSwitchStartTime(
            base::TimeTicks::Now(),
            resource_coordinator::ResourceCoordinatorTabHelper::IsLoaded(
                selection.new_contents));
      }

      if (base::FeatureList::IsEnabled(media::kEnableTabMuting)) {
        // Show the in-product help dialog pointing users to the tab mute button
        // if the user backgrounds an audible tab.
        if (selection.old_contents &&
            selection.old_contents->IsCurrentlyAudible()) {
          Browser* browser = chrome::FindBrowserWithTab(selection.old_contents);
          DCHECK(browser);
          browser->window()->MaybeShowFeaturePromo(
              feature_engagement::kIPHTabAudioMutingFeature);
        }
      }
    }
    TabStripModelChange change;
    auto visibility_tracker = InstallRenderWidgetVisibilityTracker(selection);
    OnChange(change, selection);
  }

  return selection;
}

void TabStripModel::SelectRelativeTab(TabRelativeDirection direction,
                                      TabStripUserGestureDetails detail) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (contents_data_.empty())
    return;

  const int start_index = active_index();
  absl::optional<tab_groups::TabGroupId> start_group =
      GetTabGroupForTab(start_index);

  // Ensure the active tab is not in a collapsed group so the while loop can
  // fallback on activating the active tab.
  DCHECK(!start_group.has_value() || !IsGroupCollapsed(start_group.value()));
  const int delta = direction == TabRelativeDirection::kNext ? 1 : -1;
  int index = (start_index + count() + delta) % count();
  absl::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  while (group.has_value() && IsGroupCollapsed(group.value())) {
    index = (index + count() + delta) % count();
    group = GetTabGroupForTab(index);
  }
  ActivateTabAt(index, detail);
}

void TabStripModel::MoveTabRelative(TabRelativeDirection direction) {
  const int offset = direction == TabRelativeDirection::kNext ? 1 : -1;
  const int current_index = active_index();
  absl::optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(current_index);

  const int first_non_pinned_tab_index = IndexOfFirstNonPinnedTab();
  const int first_valid_index =
      IsTabPinned(current_index) ? 0 : first_non_pinned_tab_index;
  const int last_valid_index =
      IsTabPinned(current_index) ? first_non_pinned_tab_index - 1 : count() - 1;
  int target_index = std::max(
      std::min(current_index + offset, last_valid_index), first_valid_index);

  // If the target index is the same as the current index, then the tab is at a
  // min/max boundary and being moved further in that direction. In that case,
  // the tab could still be ungrouped to move one more slot.
  const absl::optional<tab_groups::TabGroupId> target_group =
      (target_index == current_index) ? absl::nullopt
                                      : GetTabGroupForTab(target_index);

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  if (group_model_ && current_group != target_group) {
    if (current_group.has_value()) {
      UngroupTab(current_index);
      return;
    } else if (target_group.has_value()) {
      // If the tab is at a group boundary and the group is collapsed, treat the
      // collapsed group as a tab and find the next available slot for the tab
      // to move to.
      const TabGroup* group = group_model_->GetTabGroup(target_group.value());
      if (group->visual_data()->is_collapsed()) {
        const gfx::Range tabs_in_group = group->ListTabs();
        target_index = direction == TabRelativeDirection::kNext
                           ? tabs_in_group.end() - 1
                           : tabs_in_group.start();
      } else {
        GroupTab(current_index, target_group.value());
        return;
      }
    }
  }
  // TODO: this needs to be updated for multi-selection.
  MoveWebContentsAt(current_index, target_index, true);
}

void TabStripModel::MoveWebContentsAtImpl(int index,
                                          int to_position,
                                          bool select_after_move) {
  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);

  CHECK_LT(index, static_cast<int>(contents_data_.size()));
  CHECK_LT(to_position, static_cast<int>(contents_data_.size()));
  std::unique_ptr<TabModel> moved_data = std::move(contents_data_[index]);
  WebContents* web_contents = moved_data->contents();
  contents_data_.erase(contents_data_.begin() + index);
  contents_data_.insert(contents_data_.begin() + to_position,
                        std::move(moved_data));

  selection_model_.Move(index, to_position, 1);
  if (!selection_model_.IsSelected(to_position) && select_after_move)
    selection_model_.SetSelectedIndex(to_position);
  selection.new_model = selection_model_;

  TabStripModelChange::Move move;
  move.contents = web_contents;
  move.from_index = index;
  move.to_index = to_position;
  TabStripModelChange change(move);
  OnChange(change, selection);
}

void TabStripModel::MoveSelectedTabsToImpl(int index,
                                           size_t start,
                                           size_t length) {
  CHECK(start < selection_model_.selected_indices().size() &&
        start + length <= selection_model_.selected_indices().size());
  size_t end = start + length;
  int count_before_index = 0;
  const ui::ListSelectionModel::SelectedIndices& sel =
      selection_model_.selected_indices();
  auto indices = std::vector<int>(sel.begin(), sel.end());

  for (size_t i = start; i < end; ++i) {
    if (indices[i] < index + count_before_index)
      count_before_index++;
  }

  // First move those before index. Any tabs before index end up moving in the
  // selection model so we use start each time through.
  int target_index = index + count_before_index;
  size_t tab_index = start;
  while (tab_index < end && indices[start] < index) {
    MoveWebContentsAtImpl(indices[start], target_index - 1, false);
    // It is necessary to re-populate selected indices because
    // MoveWebContetsAtImpl mutates selection_model_.
    const auto& new_sel = selection_model_.selected_indices();
    indices = std::vector<int>(new_sel.begin(), new_sel.end());
    tab_index++;
  }

  // Then move those after the index. These don't result in reordering the
  // selection, therefore there is no need to repopulate indices.
  while (tab_index < end) {
    if (indices[tab_index] != target_index) {
      MoveWebContentsAtImpl(indices[tab_index], target_index, false);
    }
    tab_index++;
    target_index++;
  }
}

void TabStripModel::AddToNewGroupImpl(const std::vector<int>& indices,
                                      const tab_groups::TabGroupId& new_group) {
  if (!group_model_)
    return;

  DCHECK(!base::Contains(contents_data_, new_group, &TabModel::group));
  group_model_->AddTabGroup(new_group, absl::nullopt);

  // Find a destination for the first tab that's not pinned or inside another
  // group. We will stack the rest of the tabs up to its right.
  int destination_index = -1;
  for (int i = indices[0]; i < count(); i++) {
    const int destination_candidate = i + 1;

    // Grouping at the end of the tabstrip is always valid.
    if (!ContainsIndex(destination_candidate)) {
      destination_index = destination_candidate;
      break;
    }

    // Grouping in the middle of pinned tabs is never valid.
    if (IsTabPinned(destination_candidate))
      continue;

    // Otherwise, grouping is valid if the destination is not in the middle of a
    // different group.
    absl::optional<tab_groups::TabGroupId> destination_group =
        GetTabGroupForTab(destination_candidate);
    if (!destination_group.has_value() ||
        destination_group != GetTabGroupForTab(indices[0])) {
      destination_index = destination_candidate;
      break;
    }
  }

  MoveTabsAndSetGroupImpl(indices, destination_index, new_group);

  // Excluding the active tab, deselect all tabs being added to the group.
  // See crbug/1301846 for more info.
  const gfx::Range tab_indices =
      group_model()->GetTabGroup(new_group)->ListTabs();
  for (auto index = tab_indices.start(); index < tab_indices.end(); ++index)
    if (active_index() != static_cast<int>(index) && IsTabSelected(index))
      ToggleSelectionAt(index);
}

void TabStripModel::AddToExistingGroupImpl(
    const std::vector<int>& indices,
    const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  // Do nothing if the "existing" group can't be found. This would only happen
  // if the existing group is closed programmatically while the user is
  // interacting with the UI - e.g. if a group close operation is started by an
  // extension while the user clicks "Add to existing group" in the context
  // menu.
  // If this happens, the browser should not crash. So here we just make it a
  // no-op, since we don't want to create unintended side effects in this rare
  // corner case.
  if (!group_model_->ContainsTabGroup(group))
    return;

  const TabGroup* group_object = group_model_->GetTabGroup(group);
  int first_tab_in_group = group_object->GetFirstTab().value();
  int last_tab_in_group = group_object->GetLastTab().value();

  // Split |new_indices| into |tabs_left_of_group| and |tabs_right_of_group| to
  // be moved to proper destination index. Directly set the group for indices
  // that are inside the group.
  std::vector<int> tabs_left_of_group;
  std::vector<int> tabs_right_of_group;
  for (int index : indices) {
    if (index >= first_tab_in_group && index <= last_tab_in_group) {
      GroupTab(index, group);
    } else if (index < first_tab_in_group) {
      tabs_left_of_group.push_back(index);
    } else {
      tabs_right_of_group.push_back(index);
    }
  }

  MoveTabsAndSetGroupImpl(tabs_left_of_group, first_tab_in_group, group);
  MoveTabsAndSetGroupImpl(tabs_right_of_group, last_tab_in_group + 1, group);
}

void TabStripModel::MoveTabsAndSetGroupImpl(
    const std::vector<int>& indices,
    int destination_index,
    absl::optional<tab_groups::TabGroupId> group) {
  if (!group_model_)
    return;

  // Some tabs will need to be moved to the right, some to the left. We need to
  // handle those separately. First, move tabs to the right, starting with the
  // rightmost tab so we don't cause other tabs we are about to move to shift.
  int numTabsMovingRight = 0;
  for (size_t i = 0; i < indices.size() && indices[i] < destination_index;
       i++) {
    numTabsMovingRight++;
  }
  for (int i = numTabsMovingRight - 1; i >= 0; i--) {
    MoveAndSetGroup(indices[i], destination_index - numTabsMovingRight + i,
                    group);
  }

  // Collect indices for tabs moving to the left.
  std::vector<int> move_left_indices;
  for (size_t i = numTabsMovingRight; i < indices.size(); i++) {
    move_left_indices.push_back(indices[i]);
  }

  // Move tabs to the left, starting with the leftmost tab.
  for (size_t i = 0; i < move_left_indices.size(); i++)
    MoveAndSetGroup(move_left_indices[i], destination_index + i, group);
}

void TabStripModel::MoveAndSetGroup(
    int index,
    int new_index,
    absl::optional<tab_groups::TabGroupId> new_group) {
  if (!group_model_)
    return;

  if (new_group.has_value()) {
    // Unpin tabs when grouping -- the states should be mutually exclusive.
    // Here we move the tab twice to ensure the tabstrip is always in a valid
    // state when observers are notified of changes.
    if (IsTabPinned(index)) {
      index = SetTabPinnedImpl(index, false);
    }

    absl::optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
    if (old_group.has_value()) {
      // TODO (1302144): We don't maintain group contiguity in this case. If
      // |index| is in the middle of |old_group|, GroupTab will notify observers
      // while |old_group| is split in twain. Simply reordering the move and
      // group actions won't do it; we'd need to move, ungroup, move, and then
      // group.
      GroupTab(index, new_group.value());
      if (index != new_index)
        MoveWebContentsAtImpl(index, new_index, false);
    } else {
      // Move the tab now so that group contiguity is preserved.
      // When grouping, this will move the tab next to |new_group|.
      if (index != new_index)
        MoveWebContentsAtImpl(index, new_index, false);
      GroupTab(new_index, new_group.value());
    }
  } else {
    // Move the tab now so that group contiguity is preserved.
    // When ungrouping, this will move the tab to the edge of |old_group|.
    if (index != new_index)
      MoveWebContentsAtImpl(index, new_index, false);
    UngroupTab(new_index);
  }
}

void TabStripModel::AddToReadLaterImpl(const std::vector<int>& indices) {
  for (int index : indices)
    delegate_->AddToReadLater(GetWebContentsAt(index));
}

absl::optional<tab_groups::TabGroupId> TabStripModel::UngroupTab(int index) {
  if (!group_model_)
    return absl::nullopt;

  absl::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  if (!group.has_value())
    return absl::nullopt;

  // Update the tab.
  contents_data_[index]->set_group(absl::nullopt);
  for (auto& observer : observers_) {
    observer.TabGroupedStateChanged(absl::nullopt,
                                    contents_data_[index]->contents(), index);
  }

  // Update the group model.
  TabGroup* tab_group = group_model_->GetTabGroup(group.value());
  tab_group->RemoveTab();
  if (tab_group->IsEmpty())
    group_model_->RemoveTabGroup(group.value());

  return group;
}

void TabStripModel::GroupTab(int index, const tab_groups::TabGroupId& group) {
  if (!group_model_)
    return;

  // Check for an old group first, so that any groups that are changed can be
  // notified appropriately.
  absl::optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
  if (old_group.has_value()) {
    if (old_group.value() == group)
      return;
    else
      UngroupTab(index);
  }
  contents_data_[index]->set_group(group);
  for (auto& observer : observers_) {
    observer.TabGroupedStateChanged(group, contents_data_[index]->contents(),
                                    index);
  }

  group_model_->GetTabGroup(group)->AddTab();
}

void TabStripModel::DisconnectSavedTabGroups(
    const std::vector<int>& indices) const {
  if (!base::FeatureList::IsEnabled(features::kTabGroupsSave)) {
    return;
  }

  SavedTabGroupKeyedService* const keyed_service =
      SavedTabGroupServiceFactory::GetForProfile(profile_);
  const SavedTabGroupModel* const stg_model = keyed_service->model();

  // Count the tabs in each group in `indices`.
  std::unordered_map<tab_groups::TabGroupId, size_t, tab_groups::TabGroupIdHash>
      tabs_per_group;
  for (const int index : indices) {
    const absl::optional<tab_groups::TabGroupId> group =
        GetTabGroupForTab(index);
    if (group.has_value() && stg_model->Contains(group.value())) {
      tabs_per_group[group.value()]++;
    }
  }

  // Disconnect each group fully contained in `indices`.
  for (const auto& [group, count] : tabs_per_group) {
    const gfx::Range grouped_tabs =
        group_model_->GetTabGroup(group)->ListTabs();
    if (grouped_tabs.length() == count) {
      keyed_service->DisconnectLocalTabGroup(group);
    }
  }
}

int TabStripModel::SetTabPinnedImpl(int index, bool pinned) {
  CHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned() == pinned)
    return index;

  // Upgroup tabs if pinning -- the states should be mutually exclusive.
  if (pinned && group_model_)
    UngroupTab(index);

  // The tab's position may have to change as the pinned tab state is changing.
  int non_pinned_tab_index = IndexOfFirstNonPinnedTab();
  contents_data_[index]->set_pinned(pinned);
  if (pinned && index != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index, false);
    index = non_pinned_tab_index;
  } else if (!pinned && index + 1 != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index - 1, false);
    index = non_pinned_tab_index - 1;
  }

  for (auto& observer : observers_) {
    observer.TabPinnedStateChanged(this, contents_data_[index]->contents(),
                                   index);
  }

  return index;
}

std::vector<int> TabStripModel::SetTabsPinned(const std::vector<int>& indices,
                                              bool pinned) {
  std::vector<int> new_indices;
  if (pinned) {
    for (int index : indices) {
      if (IsTabPinned(index)) {
        new_indices.push_back(index);
      } else {
        new_indices.push_back(SetTabPinnedImpl(index, true));
      }
    }
  } else {
    for (int index : base::Reversed(indices)) {
      if (!IsTabPinned(index)) {
        new_indices.push_back(index);
      } else {
        new_indices.push_back(SetTabPinnedImpl(index, false));
      }
    }
    std::reverse(new_indices.begin(), new_indices.end());
  }
  return new_indices;
}

// Sets the sound content setting for each site at the |indices|.
void TabStripModel::SetSitesMuted(const std::vector<int>& indices,
                                  bool mute) const {
  for (int tab_index : indices) {
    content::WebContents* web_contents = GetWebContentsAt(tab_index);
    GURL url = web_contents->GetLastCommittedURL();

    // `GetLastCommittedURL` could return an empty URL if no navigation has
    // occurred yet.
    if (url.is_empty())
      continue;

    if (url.SchemeIs(content::kChromeUIScheme)) {
      // chrome:// URLs don't have content settings but can be muted, so just
      // mute the WebContents.
      chrome::SetTabAudioMuted(web_contents, mute,
                               TabMutedReason::CONTENT_SETTING_CHROME,
                               std::string());
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
  WebContents* old_contents = GetWebContentsAtImpl(index);
  WebContents* new_opener = GetOpenerOfWebContentsAt(index);

  for (auto& data : contents_data_) {
    if (data->opener() != old_contents)
      continue;

    // Ensure a tab isn't its own opener.
    data->set_opener(new_opener == data->contents() ? nullptr : new_opener);
  }

  // Sanity check that none of the tabs' openers refer |old_contents| or
  // themselves.
  DCHECK(!base::ranges::any_of(
      contents_data_, [old_contents](const std::unique_ptr<TabModel>& data) {
        return data->opener() == old_contents ||
               data->opener() == data->contents();
      }));
}

void TabStripModel::EnsureGroupContiguity(int index) {
  if (!group_model_)
    return;

  const auto old_group = GetTabGroupForTab(index);
  const auto new_left_group = GetTabGroupForTab(index - 1);
  const auto new_right_group = GetTabGroupForTab(index + 1);

  if (old_group != new_left_group && old_group != new_right_group) {
    if (new_left_group == new_right_group && new_left_group.has_value()) {
      // The tab is in the middle of an existing group, so add it to that group.
      GroupTab(index, new_left_group.value());
    } else if (old_group.has_value() &&
               group_model_->GetTabGroup(old_group.value())->tab_count() > 1) {
      // The tab is between groups and its group is non-contiguous, so clear
      // this tab's group.
      UngroupTab(index);
    }
  }
}

int TabStripModel::GetTabIndexAfterClosing(int index,
                                           int removing_index) const {
  if (removing_index < index)
    index = std::max(0, index - 1);
  return index;
}

void TabStripModel::OnActiveTabChanged(
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || empty())
    return;

  content::WebContents* old_contents = selection.old_contents;
  content::WebContents* new_contents = selection.new_contents;
  content::WebContents* old_opener = nullptr;
  int reason = selection.reason;

  if (old_contents) {
    int index = GetIndexOfWebContents(old_contents);
    if (index != TabStripModel::kNoTab) {
      // When switching away from a tab, the tab preview system may want to
      // capture an updated preview image. This must be done before any changes
      // are made to the old contents, and while the contents are still visible.
      //
      // It's possible this could be done with a separate TabStripModelObserver,
      // but then it would be possible for a different observer to jump in front
      // and modify the WebContents, so for now, do it here.
      auto* const thumbnail_helper =
          ThumbnailTabHelper::FromWebContents(old_contents);
      if (thumbnail_helper) {
        thumbnail_helper->CaptureThumbnailOnTabBackgrounded();
      }

      old_opener = GetOpenerOfWebContentsAt(index);

      // Forget the opener relationship if it needs to be reset whenever the
      // active tab changes (see comment in TabStripModel::AddWebContents, where
      // the flag is set).
      if (contents_data_[index]->reset_opener_on_active_tab_change())
        ForgetOpener(old_contents);
    }
  }
  DCHECK(selection.new_model.active().has_value());
  content::WebContents* new_opener =
      GetOpenerOfWebContentsAt(selection.new_model.active().value());

  if ((reason & TabStripModelObserver::CHANGE_REASON_USER_GESTURE) &&
      new_opener != old_opener &&
      ((old_contents == nullptr && new_opener == nullptr) ||
       new_opener != old_contents) &&
      ((new_contents == nullptr && old_opener == nullptr) ||
       old_opener != new_contents)) {
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
  if (!tab_count)
    return 0;

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
    if (index == TabStripModel::kNoTab)
      return active_index() + 1;

    // Normally we'd add the tab immediately after the most recent tab
    // associated with `opener`. However, if there is a group discontinuity
    // between the active tab and where we'd like to place the tab, we'll place
    // it just before the discontinuity instead (see crbug.com/1246421).
    const auto opener_group = GetTabGroupForTab(active_index());
    for (int i = active_index() + 1; i <= index; ++i) {
      // Insert before the first tab that differs in group.
      if (GetTabGroupForTab(i) != opener_group)
        return i;
    }
    // If there is no discontinuity, add after the last tab already associated
    // with the opener.
    return index + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return count();
}

absl::optional<int> TabStripModel::DetermineNewSelectedIndex(
    int removing_index) const {
  DCHECK(ContainsIndex(removing_index));

  if (removing_index != active_index())
    return absl::nullopt;

  if (selection_model().size() > 1)
    return absl::nullopt;

  content::WebContents* parent_opener =
      GetOpenerOfWebContentsAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first that child opened, not the next tab opened by the
  // removed tab.
  content::WebContents* removed_contents = GetWebContentsAt(removing_index);
  // The parent opener should never be the same as the controller being removed.
  DCHECK(parent_opener != removed_contents);
  int index =
      GetIndexOfNextWebContentsOpenedBy(removed_contents, removing_index);
  if (index != TabStripModel::kNoTab && !IsTabCollapsed(index))
    return GetTabIndexAfterClosing(index, removing_index);

  if (parent_opener) {
    // If the tab has an opener, shift selection to the next tab with the same
    // opener.
    index = GetIndexOfNextWebContentsOpenedBy(parent_opener, removing_index);
    if (index != TabStripModel::kNoTab && !IsTabCollapsed(index))
      return GetTabIndexAfterClosing(index, removing_index);

    // If we can't find another tab with the same opener, fall back to the
    // opener itself.
    index = GetIndexOfWebContents(parent_opener);
    if (index != TabStripModel::kNoTab && !IsTabCollapsed(index))
      return GetTabIndexAfterClosing(index, removing_index);
  }

  // If closing a grouped tab, return a tab that is still in the group, if any.
  const absl::optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(removing_index);
  if (current_group.has_value()) {
    // Match the default behavior below: prefer the tab to the right.
    const absl::optional<tab_groups::TabGroupId> right_group =
        GetTabGroupForTab(removing_index + 1);
    if (current_group == right_group)
      return removing_index;

    const absl::optional<tab_groups::TabGroupId> left_group =
        GetTabGroupForTab(removing_index - 1);
    if (current_group == left_group)
      return removing_index - 1;
  }

  // At this point, the tab detaching is either not inside a group, or the last
  // tab in the group. If there are any tabs in a not collapsed group,
  // |GetNextExpandedActiveTab()| will return the index of that tab.
  absl::optional<int> next_available =
      GetNextExpandedActiveTab(removing_index, absl::nullopt);
  if (next_available.has_value())
    return GetTabIndexAfterClosing(next_available.value(), removing_index);

  // By default, return the tab on the right, unless this is the last tab.
  // Reaching this point means there are no other tabs in an uncollapsed group.
  // The tab at the specified index will become automatically expanded by the
  // caller.
  if (removing_index >= (count() - 1))
    return removing_index - 1;

  return removing_index;
}
