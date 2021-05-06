// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_elider.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

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
  bool* const guard_flag_;
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

// Intalls RenderWidgetVisibilityTracker when the active tab has changed.
std::unique_ptr<RenderWidgetHostVisibilityTracker>
InstallRenderWigetVisibilityTracker(const TabStripSelectionChange& selection) {
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
class RenderWidgetHostVisibilityTracker
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
  ~RenderWidgetHostVisibilityTracker() final = default;
  RenderWidgetHostVisibilityTracker(const RenderWidgetHostVisibilityTracker&) =
      delete;
  RenderWidgetHostVisibilityTracker& operator=(
      const RenderWidgetHostVisibilityTracker&) = delete;

 private:
  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* host,
                                         bool became_visible) override {
    DCHECK(became_visible);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Browser.Tabs.SelectionToVisibilityRequestTime", timer_.Elapsed(),
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(3),
        50);
    TRACE_EVENT_NESTABLE_ASYNC_END0("ui,latency", "TabSwitchVisibilityRequest",
                                    this);
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

///////////////////////////////////////////////////////////////////////////////
// WebContentsData

// An object to own a WebContents that is in a tabstrip, as well as other
// various properties it has.
class TabStripModel::WebContentsData : public content::WebContentsObserver {
 public:
  explicit WebContentsData(std::unique_ptr<WebContents> a_contents);
  WebContentsData(const WebContentsData&) = delete;
  WebContentsData& operator=(const WebContentsData&) = delete;

  // Changes the WebContents that this WebContentsData tracks.
  std::unique_ptr<WebContents> ReplaceWebContents(
      std::unique_ptr<WebContents> contents);
  WebContents* web_contents() { return contents_.get(); }

  // See comments on fields.
  WebContents* opener() const { return opener_; }
  void set_opener(WebContents* value) {
    DCHECK_NE(value, web_contents()) << "A tab should not be its own opener.";
    opener_ = value;
  }
  void set_reset_opener_on_active_tab_change(bool value) {
    reset_opener_on_active_tab_change_ = value;
  }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool pinned() const { return pinned_; }
  void set_pinned(bool value) { pinned_ = value; }
  bool blocked() const { return blocked_; }
  void set_blocked(bool value) { blocked_ = value; }
  base::Optional<tab_groups::TabGroupId> group() const { return group_; }
  void set_group(base::Optional<tab_groups::TabGroupId> value) {
    group_ = value;
  }

  void WriteIntoTracedValue(perfetto::TracedValue context) const {
    auto dict = std::move(context).WriteDictionary();
    dict.Add("web_contents", contents_);
    dict.Add("pinned", pinned_);
    dict.Add("blocked", blocked_);
  }

 private:
  // Make sure that if someone deletes this WebContents out from under us, it
  // is properly removed from the tab strip.
  void WebContentsDestroyed() override;

  // The WebContents owned by this WebContentsData.
  std::unique_ptr<WebContents> contents_;

  // The opener is used to model a set of tabs spawned from a single parent tab.
  // The relationship is discarded easily, e.g. when the user switches to a tab
  // not part of the set. This property is used to determine what tab to
  // activate next when one is closed.
  WebContents* opener_ = nullptr;

  // True if |opener_| should be reset when any active tab change occurs (rather
  // than just one outside the current tree of openers).
  bool reset_opener_on_active_tab_change_ = false;

  // Whether the tab is pinned.
  bool pinned_ = false;

  // Whether the tab interaction is blocked by a modal dialog.
  bool blocked_ = false;

  // The group that contains this tab, if any.
  base::Optional<tab_groups::TabGroupId> group_ = base::nullopt;
};

TabStripModel::WebContentsData::WebContentsData(
    std::unique_ptr<WebContents> contents)
    : content::WebContentsObserver(contents.get()),
      contents_(std::move(contents)) {}

std::unique_ptr<WebContents> TabStripModel::WebContentsData::ReplaceWebContents(
    std::unique_ptr<WebContents> contents) {
  contents_.swap(contents);
  Observe(contents_.get());
  return contents;
}

void TabStripModel::WebContentsData::WebContentsDestroyed() {
  // TODO(erikchen): Remove this NOTREACHED statement as well as the
  // WebContents observer - this is just a temporary sanity check to make sure
  // that unit tests are not destroyed a WebContents out from under a
  // TabStripModel.
  NOTREACHED();
}

// Holds state for a WebContents that has been detached from the tab strip. Will
// also handle WebContents deletion if |will_delete| is true.
struct TabStripModel::DetachedWebContents {
  DetachedWebContents(int index_before_any_removals,
                      int index_at_time_of_removal,
                      std::unique_ptr<WebContents> contents,
                      bool will_delete)
      : contents(std::move(contents)),
        index_before_any_removals(index_before_any_removals),
        index_at_time_of_removal(index_at_time_of_removal),
        will_delete(will_delete) {}
  DetachedWebContents(const DetachedWebContents&) = delete;
  DetachedWebContents& operator=(const DetachedWebContents&) = delete;
  ~DetachedWebContents() = default;
  DetachedWebContents(DetachedWebContents&&) = default;

  std::unique_ptr<WebContents> contents;

  // The index of the WebContents in the original selection model of the tab
  // strip [prior to any tabs being removed, if multiple tabs are being
  // simultaneously removed].
  const int index_before_any_removals;

  // The index of the WebContents at the time it is being removed. If multiple
  // tabs are being simultaneously removed, the index reflects previously
  // removed tabs in this batch.
  const int index_at_time_of_removal;

  // Whether to delete the WebContents after sending notifications.
  const bool will_delete;
};

// Holds all state necessary to send notifications for detached tabs.
struct TabStripModel::DetachNotifications {
  DetachNotifications(WebContents* initially_active_web_contents,
                      const ui::ListSelectionModel& selection_model)
      : initially_active_web_contents(initially_active_web_contents),
        selection_model(selection_model) {}
  DetachNotifications(const DetachNotifications&) = delete;
  DetachNotifications& operator=(const DetachNotifications&) = delete;
  ~DetachNotifications() = default;

  // The WebContents that was active prior to any detaches happening.
  //
  // It's safe to use a raw pointer here because the active web contents, if
  // detached, is owned by |detached_web_contents|.
  //
  // Once the notification for change of active web contents has been sent,
  // this field is set to nullptr.
  WebContents* initially_active_web_contents = nullptr;

  // The WebContents that were recently detached. Observers need to be notified
  // about these. These must be updated after construction.
  std::vector<std::unique_ptr<DetachedWebContents>> detached_web_contents;

  // The selection model prior to any tabs being detached.
  const ui::ListSelectionModel selection_model;
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

constexpr int TabStripModel::kNoTab;

TabStripModel::TabStripModel(TabStripModelDelegate* delegate, Profile* profile)
    : delegate_(delegate), profile_(profile) {
  DCHECK(delegate_);
  order_controller_ = std::make_unique<TabStripModelOrderController>(this);
  group_model_ = std::make_unique<TabGroupModel>(this);

  constexpr base::TimeDelta kTabScrubbingHistogramIntervalTime =
      base::TimeDelta::FromSeconds(30);

  last_tab_switch_timestamp_ = base::TimeTicks::Now();
  tab_scrubbing_interval_timer_.Start(
      FROM_HERE, kTabScrubbingHistogramIntervalTime,
      base::BindRepeating(&TabStripModel::RecordTabScrubbingMetrics,
                          base::Unretained(this)));
}

TabStripModel::~TabStripModel() {
  std::vector<TabStripModelObserver*> observers;
  for (auto& observer : observers_)
    observer.ModelDestroyed(TabStripModelObserver::ModelPasskey(), this);

  contents_data_.clear();
  order_controller_.reset();
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
    base::Optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  return InsertWebContentsAtImpl(index, std::move(contents), add_types, group);
}

std::unique_ptr<content::WebContents> TabStripModel::ReplaceWebContentsAt(
    int index,
    std::unique_ptr<WebContents> new_contents) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  delegate()->WillAddWebContents(new_contents.get());

  DCHECK(ContainsIndex(index));

  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);
  WebContents* raw_new_contents = new_contents.get();
  std::unique_ptr<WebContents> old_contents =
      contents_data_[index]->ReplaceWebContents(std::move(new_contents));

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
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);

  return old_contents;
}

std::unique_ptr<content::WebContents> TabStripModel::DetachWebContentsAt(
    int index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  DCHECK_NE(active_index(), kNoTab) << "Activate the TabStripModel by "
                                       "selecting at least one tab before "
                                       "trying to detach web contents.";
  WebContents* initially_active_web_contents =
      GetWebContentsAtImpl(active_index());

  DetachNotifications notifications(initially_active_web_contents,
                                    selection_model_);
  std::unique_ptr<DetachedWebContents> dwc =
      std::make_unique<DetachedWebContents>(
          index, index,
          DetachWebContentsImpl(index, /*create_historical_tab=*/false),
          /*will_delete=*/false);
  notifications.detached_web_contents.push_back(std::move(dwc));
  SendDetachWebContentsNotifications(&notifications);
  return std::move(notifications.detached_web_contents[0]->contents);
}

std::unique_ptr<content::WebContents> TabStripModel::DetachWebContentsImpl(
    int index,
    bool create_historical_tab) {
  if (contents_data_.empty())
    return nullptr;
  DCHECK(ContainsIndex(index));

  FixOpeners(index);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database.
  WebContents* raw_web_contents = GetWebContentsAtImpl(index);
  if (create_historical_tab)
    delegate_->CreateHistoricalTab(raw_web_contents);

  base::Optional<int> next_selected_index =
      order_controller_->DetermineNewSelectedIndex(index);

  UngroupTab(index);

  std::unique_ptr<WebContentsData> old_data = std::move(contents_data_[index]);
  contents_data_.erase(contents_data_.begin() + index);

  if (empty()) {
    selection_model_.Clear();
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index);
    ui::ListSelectionModel old_model;
    old_model = selection_model_;
    if (index == old_active) {
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
  return old_data->ReplaceWebContents(nullptr);
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
    remove.contents.push_back({dwc->contents.get(),
                               dwc->index_before_any_removals,
                               dwc->will_delete});
  }
  TabStripModelChange change(std::move(remove));

  TabStripSelectionChange selection;
  selection.old_contents = notifications->initially_active_web_contents;
  selection.new_contents = GetActiveWebContents();
  selection.old_model = notifications->selection_model;
  selection.new_model = selection_model_;
  selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
  selection.selected_tabs_were_removed = std::any_of(
      notifications->detached_web_contents.begin(),
      notifications->detached_web_contents.end(), [&notifications](auto& dwc) {
        return notifications->selection_model.IsSelected(
            dwc->index_before_any_removals);
      });

  {
    auto visibility_tracker =
        empty() ? nullptr : InstallRenderWigetVisibilityTracker(selection);
    for (auto& observer : observers_)
      observer.OnTabStripModelChanged(this, change, selection);
  }

  for (auto& dwc : notifications->detached_web_contents) {
    if (dwc->will_delete) {
      // This destroys the WebContents, which will also send
      // WebContentsDestroyed notifications.
      dwc->contents.reset();
    }
  }

  if (empty()) {
    for (auto& observer : observers_)
      observer.TabStripEmpty();
  }
}

void TabStripModel::ActivateTabAt(int index, UserGestureDetails user_gesture) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  DCHECK(ContainsIndex(index));
  TRACE_EVENT0("ui", "TabStripModel::ActivateTabAt");

  // Maybe increment count of tabs 'scrubbed' by mouse or key press for
  // histogram data.
  if (user_gesture.type == GestureType::kMouse ||
      user_gesture.type == GestureType::kKeyboard) {
    constexpr base::TimeDelta kMaxTimeConsideredScrubbing =
        base::TimeDelta::FromMilliseconds(1500);
    base::TimeDelta elapsed_time_since_tab_switch =
        base::TimeTicks::Now() - last_tab_switch_timestamp_;
    if (elapsed_time_since_tab_switch <= kMaxTimeConsideredScrubbing) {
      if (user_gesture.type == GestureType::kMouse)
        ++tabs_scrubbed_by_mouse_press_count_;
      else if (user_gesture.type == GestureType::kKeyboard)
        ++tabs_scrubbed_by_key_press_count_;
    }
  }
  last_tab_switch_timestamp_ = base::TimeTicks::Now();

  TabSwitchEventLatencyRecorder::EventType event_type;
  switch (user_gesture.type) {
    case GestureType::kMouse:
      event_type = TabSwitchEventLatencyRecorder::EventType::kMouse;
      break;
    case GestureType::kKeyboard:
      event_type = TabSwitchEventLatencyRecorder::EventType::kKeyboard;
      break;
    case GestureType::kTouch:
      event_type = TabSwitchEventLatencyRecorder::EventType::kTouch;
      break;
    case GestureType::kWheel:
      event_type = TabSwitchEventLatencyRecorder::EventType::kWheel;
      break;
    default:
      event_type = TabSwitchEventLatencyRecorder::EventType::kOther;
      break;
  }
  tab_switch_event_latency_recorder_.BeginLatencyTiming(user_gesture.time_stamp,
                                                        event_type);
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
  SetSelection(std::move(new_model),
               user_gesture.type != GestureType::kNone
                   ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
                   : TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

void TabStripModel::RecordTabScrubbingMetrics() {
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.MousePress",
                             tabs_scrubbed_by_mouse_press_count_);
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.KeyPress",
                             tabs_scrubbed_by_key_press_count_);
  tabs_scrubbed_by_mouse_press_count_ = 0;
  tabs_scrubbed_by_key_press_count_ = 0;
}

int TabStripModel::MoveWebContentsAt(int index,
                                     int to_position,
                                     bool select_after_move) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  DCHECK(ContainsIndex(index));

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

  DCHECK_NE(to_index, kNoTab);

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
    if (contents_data_[i]->web_contents() == contents)
      return i;
  }
  return kNoTab;
}

void TabStripModel::UpdateWebContentsStateAt(int index,
                                             TabChangeType change_type) {
  DCHECK(ContainsIndex(index));

  for (auto& observer : observers_)
    observer.TabChangedAt(GetWebContentsAtImpl(index), index, change_type);
}

void TabStripModel::SetTabNeedsAttentionAt(int index, bool attention) {
  DCHECK(ContainsIndex(index));

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
  InternalCloseTabs(closing_tabs, CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseAllTabsInGroup(const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  delegate_->CreateHistoricalGroup(group);

  gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  if (static_cast<int>(tabs_in_group.length()) == count())
    closing_all_ = true;

  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(tabs_in_group.length());
  for (uint32_t i = tabs_in_group.end(); i > tabs_in_group.start(); --i)
    closing_tabs.push_back(GetWebContentsAt(i - 1));
  InternalCloseTabs(closing_tabs, CLOSE_CREATE_HISTORICAL_TAB);
}

bool TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  DCHECK(ContainsIndex(index));
  WebContents* contents = GetWebContentsAt(index);
  return InternalCloseTabs(base::span<WebContents* const>(&contents, 1),
                           close_types);
}

bool TabStripModel::TabsAreLoading() const {
  for (const auto& data : contents_data_) {
    if (data->web_contents()->IsLoading())
      return true;
  }

  return false;
}

WebContents* TabStripModel::GetOpenerOfWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->opener();
}

void TabStripModel::SetOpenerOfWebContentsAt(int index, WebContents* opener) {
  DCHECK(ContainsIndex(index));
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
  DCHECK(ContainsIndex(start_index));

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
    opener_and_descendants.insert(contents_data_[i]->web_contents());
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
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->blocked() == blocked)
    return;
  contents_data_[index]->set_blocked(blocked);
  for (auto& observer : observers_)
    observer.TabBlockedStateChanged(contents_data_[index]->web_contents(),
                                    index);
}

void TabStripModel::SetTabPinned(int index, bool pinned) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  SetTabPinnedImpl(index, pinned);
}

bool TabStripModel::IsTabPinned(int index) const {
  DCHECK(ContainsIndex(index)) << index;
  return contents_data_[index]->pinned();
}

bool TabStripModel::IsTabCollapsed(int index) const {
  base::Optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  return group.has_value() && IsGroupCollapsed(group.value());
}

bool TabStripModel::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  return group_model()->ContainsTabGroup(group) &&
         group_model()->GetTabGroup(group)->visual_data()->is_collapsed();
}

bool TabStripModel::IsTabBlocked(int index) const {
  return contents_data_[index]->blocked();
}

base::Optional<tab_groups::TabGroupId> TabStripModel::GetTabGroupForTab(
    int index) const {
  return ContainsIndex(index) ? contents_data_[index]->group() : base::nullopt;
}

base::Optional<tab_groups::TabGroupId> TabStripModel::GetSurroundingTabGroup(
    int index) const {
  if (!ContainsIndex(index - 1) || !ContainsIndex(index))
    return base::nullopt;

  // If the tab before is not in a group, a tab inserted at |index|
  // wouldn't be surrounded by one group.
  base::Optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index - 1);
  if (!group)
    return base::nullopt;

  // If the tab after is in a different (or no) group, a new tab at
  // |index| isn't surrounded.
  if (group != GetTabGroupForTab(index))
    return base::nullopt;
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
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::ToggleSelectionAt(int index) {
  if (!delegate()->CanHighlightTabs())
    return false;
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model();
  if (selection_model_.IsSelected(index)) {
    if (selection_model_.size() == 1) {
      // One tab must be selected and this tab is currently selected so we can't
      // unselect it.
      return false;
    }
    new_model.RemoveIndexFromSelection(index);
    new_model.set_anchor(index);
    if (new_model.active() == index ||
        new_model.active() == ui::ListSelectionModel::kUnselectedIndex)
      new_model.set_active(*new_model.selected_indices().begin());
  } else {
    new_model.AddIndexToSelection(index);
    new_model.set_anchor(index);
    new_model.set_active(index);
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
  DCHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModel::SetSelectionFromModel(ui::ListSelectionModel source) {
  DCHECK_NE(ui::ListSelectionModel::kUnselectedIndex, source.active());
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
    base::Optional<tab_groups::TabGroupId> group) {
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
    index = order_controller_->DetermineInsertionIndex(transition,
                                                       add_types & ADD_ACTIVE);
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
  if (group.has_value()) {
    gfx::Range grouped_tabs =
        group_model_->GetTabGroup(group.value())->ListTabs();
    if (grouped_tabs.length() > 0) {
      index = base::ClampToRange(index, static_cast<int>(grouped_tabs.start()),
                                 static_cast<int>(grouped_tabs.end()));
    }
  } else if (GetTabGroupForTab(index - 1) == GetTabGroupForTab(index)) {
    group = GetTabGroupForTab(index);
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
  InternalCloseTabs(
      GetWebContentsesByIndices(std::vector<int>(sel.begin(), sel.end())),
      CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
}

void TabStripModel::SelectNextTab(UserGestureDetails detail) {
  SelectRelativeTab(true, detail);
}

void TabStripModel::SelectPreviousTab(UserGestureDetails detail) {
  SelectRelativeTab(false, detail);
}

void TabStripModel::SelectLastTab(UserGestureDetails detail) {
  ActivateTabAt(count() - 1, detail);
}

void TabStripModel::MoveTabNext() {
  MoveTabRelative(true);
}

void TabStripModel::MoveTabPrevious() {
  MoveTabRelative(false);
}

tab_groups::TabGroupId TabStripModel::AddToNewGroup(
    const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Ensure that the indices are sorted and unique.
  DCHECK(base::ranges::is_sorted(indices));
  DCHECK(std::adjacent_find(indices.begin(), indices.end()) == indices.end());

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

  // Ensure that the indices are sorted and unique.
  DCHECK(base::ranges::is_sorted(indices));
  DCHECK(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
  DCHECK(ContainsIndex(*(indices.begin())));
  DCHECK(ContainsIndex(*(indices.rbegin())));

  AddToExistingGroupImpl(indices, group);
}

void TabStripModel::MoveTabsAndSetGroup(
    const std::vector<int>& indices,
    int destination_index,
    base::Optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  MoveTabsAndSetGroupImpl(indices, destination_index, group);
}

void TabStripModel::AddToGroupForRestore(const std::vector<int>& indices,
                                         const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  const bool group_exists = group_model_->ContainsTabGroup(group);
  if (group_exists)
    AddToExistingGroupImpl(indices, group);
  else
    AddToNewGroupImpl(indices, group);
}

void TabStripModel::UpdateGroupForDragRevert(
    int index,
    base::Optional<tab_groups::TabGroupId> group_id,
    base::Optional<tab_groups::TabGroupVisualData> group_data) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
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

  std::map<tab_groups::TabGroupId, std::vector<int>> indices_per_tab_group;

  for (int index : indices) {
    base::Optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
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
    MoveTabsAndSetGroupImpl(left_of_group, first_tab_in_group, base::nullopt);
    MoveTabsAndSetGroupImpl(right_of_group, last_tab_in_group + 1,
                            base::nullopt);
  }
}

bool TabStripModel::IsReadLaterSupportedForAny(const std::vector<int> indices) {
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
  TabGroupChange change(group, TabGroupChange::kCreated);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::OpenTabGroupEditor(const tab_groups::TabGroupId& group) {
  TabGroupChange change(group, TabGroupChange::kEditorOpened);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::ChangeTabGroupContents(
    const tab_groups::TabGroupId& group) {
  TabGroupChange change(group, TabGroupChange::kContentsChanged);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::ChangeTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const TabGroupChange::VisualsChange& visuals) {
  TabGroupChange change(group, visuals);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::MoveTabGroup(const tab_groups::TabGroupId& group) {
  TabGroupChange change(group, TabGroupChange::kMoved);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
}

void TabStripModel::CloseTabGroup(const tab_groups::TabGroupId& group) {
  TabGroupChange change(group, TabGroupChange::kClosed);
  for (auto& observer : observers_)
    observer.OnTabGroupChanged(change);
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

    case CommandReload: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          Browser* browser = chrome::FindBrowserWithWebContents(tab);
          if (!browser || browser->CanReloadContents(tab))
            return true;
        }
      }
      return false;
    }

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight:
      return !GetIndicesClosedByCommand(context_index, command_id).empty();

    case CommandDuplicate: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (delegate()->CanDuplicateContentsAt(indices[i]))
          return true;
      }
      return false;
    }

    case CommandToggleSiteMuted:
      return true;

    case CommandTogglePinned:
      return true;

    case CommandToggleGrouped:
      return true;

    case CommandFocusMode:
      return GetIndicesForCommand(context_index).size() == 1;

    case CommandSendTabToSelf:
      return true;

    case CommandSendTabToSelfSingleTarget:
      return true;

    case CommandAddToReadLater:
      return true;

    case CommandAddToNewGroup:
      return true;

    case CommandAddToExistingGroup:
      return true;

    case CommandRemoveFromGroup:
      return true;

    case CommandMoveToExistingWindow:
      return true;

    case CommandMoveTabsToNewWindow: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      const bool would_leave_strip_empty =
          static_cast<int>(indices.size()) == count();
      return !would_leave_strip_empty &&
             delegate()->CanMoveTabsToWindow(indices);
    }

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
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
                                TabStripModel::NEW_TAB_CONTEXT_MENU,
                                TabStripModel::NEW_TAB_ENUM_COUNT);
      delegate()->AddTabAt(GURL(), context_index + 1, true,
                           GetTabGroupForTab(context_index));
      break;
    }

    case CommandReload: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          Browser* browser = chrome::FindBrowserWithWebContents(tab);
          if (!browser || browser->CanReloadContents(tab))
            tab->GetController().Reload(content::ReloadType::NORMAL, true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the WebContents off as the indices will change as tabs are
      // duplicated.
      std::vector<WebContents*> tabs;
      for (size_t i = 0; i < indices.size(); ++i)
        tabs.push_back(GetWebContentsAt(indices[i]));
      for (size_t i = 0; i < tabs.size(); ++i) {
        int index = GetIndexOfWebContents(tabs[i]);
        if (index != -1 && delegate()->CanDuplicateContentsAt(index))
          delegate()->DuplicateContentsAt(index);
      }
      break;
    }

    case CommandCloseTab: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      InternalCloseTabs(
          GetWebContentsesByIndices(GetIndicesForCommand(context_index)),
          CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
      break;
    }

    case CommandCloseOtherTabs: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandCloseTabsToRight: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandSendTabToSelfSingleTarget: {
      send_tab_to_self::ShareToSingleTarget(GetWebContentsAt(context_index));
      send_tab_to_self::RecordSendTabToSelfClickResult(
          send_tab_to_self::kTabMenu, SendTabToSelfClickResult::kClickItem);
      break;
    }

    case CommandTogglePinned: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool pin = WillContextMenuPin(context_index);
      if (pin) {
        for (size_t i = 0; i < indices.size(); ++i)
          SetTabPinnedImpl(indices[i], true);
      } else {
        // Unpin from the back so that the order is maintained (unpinning can
        // trigger moving a tab).
        for (size_t i = indices.size(); i > 0; --i)
          SetTabPinnedImpl(indices[i - 1], false);
      }
      break;
    }

    case CommandToggleGrouped: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool group = WillContextMenuGroup(context_index);
      if (group) {
        tab_groups::TabGroupId new_group = AddToNewGroup(indices);
        OpenTabGroupEditor(new_group);
      } else {
        RemoveFromGroup(indices);
      }

      break;
    }

    case CommandFocusMode: {
      base::RecordAction(UserMetricsAction("TabContextMenu_FocusMode"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      WebContents* contents = GetWebContentsAt(indices[0]);
      web_app::ReparentWebContentsForFocusMode(contents);
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

    case CommandAddToReadLater: {
      base::RecordAction(
          UserMetricsAction("DesktopReadingList.AddItem.FromTabContextMenu"));
      AddToReadLater(GetIndicesForCommand(context_index));
      break;
    }

    case CommandAddToNewGroup: {
      base::RecordAction(UserMetricsAction("TabContextMenu_AddToNewGroup"));

      tab_groups::TabGroupId new_group =
          AddToNewGroup(GetIndicesForCommand(context_index));
      OpenTabGroupEditor(new_group);
      break;
    }

    case CommandAddToExistingGroup: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingGroupCommand with the correct group later.
      break;
    }

    case CommandRemoveFromGroup: {
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

    default:
      NOTREACHED();
  }
}

void TabStripModel::ExecuteAddToExistingGroupCommand(
    int context_index,
    const tab_groups::TabGroupId& group) {
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

std::vector<std::u16string> TabStripModel::GetExistingWindowsForMoveMenu() {
  return delegate()->GetExistingWindowsForMoveMenu();
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
  std::vector<int> indices = GetIndicesForCommand(index);
  DCHECK(!indices.empty());

  // If all tabs are in the same group, then we ungroup, otherwise we group.
  base::Optional<tab_groups::TabGroupId> group = GetTabGroupForTab(indices[0]);
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
    case CommandSendTabToSelfSingleTarget:
      *browser_cmd = IDC_SEND_TAB_TO_SELF_SINGLE_TARGET;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandFocusMode:
      *browser_cmd = IDC_FOCUS_THIS_TAB;
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
  DCHECK(ContainsIndex(start_index));

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

base::Optional<int> TabStripModel::GetNextExpandedActiveTab(
    int start_index,
    base::Optional<tab_groups::TabGroupId> collapsing_group) const {
  // Check tabs from the start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    base::Optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    base::Optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  return base::nullopt;
}

void TabStripModel::ForgetAllOpeners() {
  for (const auto& data : contents_data_)
    data->set_opener(nullptr);
}

void TabStripModel::ForgetOpener(WebContents* contents) {
  const int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  contents_data_[index]->set_opener(nullptr);
}

bool TabStripModel::ShouldResetOpenerOnActiveTabChange(
    WebContents* contents) const {
  const int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->reset_opener_on_active_tab_change();
}

void TabStripModel::WriteIntoTracedValue(perfetto::TracedValue context) const {
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
  return pinned_tab
             ? base::ClampToRange(index, 0, IndexOfFirstNonPinnedTab())
             : base::ClampToRange(index, IndexOfFirstNonPinnedTab(), count());
}

int TabStripModel::ConstrainMoveIndex(int index, bool pinned_tab) const {
  return pinned_tab
             ? base::ClampToRange(index, 0, IndexOfFirstNonPinnedTab() - 1)
             : base::ClampToRange(index, IndexOfFirstNonPinnedTab(),
                                  count() - 1);
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
  DCHECK(ContainsIndex(index));
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
    base::Optional<tab_groups::TabGroupId> group) {
  delegate()->WillAddWebContents(contents.get());

  bool active = (add_types & ADD_ACTIVE) != 0;
  bool pin = (add_types & ADD_PINNED) != 0;
  index = ConstrainInsertionIndex(index, pin);

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  WebContents* active_contents = GetActiveWebContents();
  WebContents* raw_contents = contents.get();
  std::unique_ptr<WebContentsData> data =
      std::make_unique<WebContentsData>(std::move(contents));
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
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);
  if (group.has_value())
    GroupTab(index, group.value());

  return index;
}

bool TabStripModel::InternalCloseTabs(
    base::span<content::WebContents* const> items,
    uint32_t close_types) {
  if (items.empty())
    return true;

  const bool closing_all = static_cast<int>(items.size()) == count();
  base::WeakPtr<TabStripModel> ref = weak_factory_.GetWeakPtr();
  if (closing_all) {
    for (auto& observer : observers_)
      observer.WillCloseAllTabs(this);
  }

  DetachNotifications notifications(GetWebContentsAtImpl(active_index()),
                                    selection_model_);
  const bool closed_all =
      CloseWebContentses(items, close_types, &notifications);

  // When unload handler is triggered for all items, we should wait for the
  // result.
  if (!notifications.detached_web_contents.empty())
    SendDetachWebContentsNotifications(&notifications);

  if (!ref)
    return closed_all;
  if (closing_all) {
    // CloseAllTabsStopped is sent with reason kCloseAllCompleted if
    // closed_all; otherwise kCloseAllCanceled is sent.
    for (auto& observer : observers_)
      observer.CloseAllTabsStopped(
          this, closed_all ? TabStripModelObserver::kCloseAllCompleted
                           : TabStripModelObserver::kCloseAllCanceled);
  }

  return closed_all;
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
          contents->GetMainFrame()->GetProcess();
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

  for (size_t i = 0; i < items.size(); ++i) {
    WebContents* closing_contents = items[i];

    // The index into contents_data_.
    int current_index = GetIndexOfWebContents(closing_contents);
    DCHECK_NE(current_index, kNoTab);

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & TabStripModel::CLOSE_USER_GESTURE);
    }

    if (RunUnloadListenerBeforeClosing(closing_contents)) {
      closed_all = false;
      continue;
    }

    std::unique_ptr<DetachedWebContents> dwc =
        std::make_unique<DetachedWebContents>(
            original_indices[i], current_index,
            DetachWebContentsImpl(current_index,
                                  close_types & CLOSE_CREATE_HISTORICAL_TAB),
            /*will_delete=*/true);
    notifications->detached_web_contents.push_back(std::move(dwc));
  }

  return closed_all;
}

WebContents* TabStripModel::GetWebContentsAtImpl(int index) const {
  CHECK(ContainsIndex(index))
      << "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_[index]->web_contents();
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
  DCHECK(ContainsIndex(new_model.active()));
  for (int selected_index : new_model.selected_indices()) {
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
      auto now = base::TimeTicks::Now();
      if (selection.new_contents &&
          selection.new_contents->GetRenderWidgetHostView()) {
        auto input_event_timestamp =
            tab_switch_event_latency_recorder_.input_event_timestamp();
        // input_event_timestamp may be null in some cases, e.g. in tests.
        selection.new_contents->GetRenderWidgetHostView()
            ->SetRecordContentToVisibleTimeRequest(
                !input_event_timestamp.is_null() ? input_event_timestamp : now,
                resource_coordinator::ResourceCoordinatorTabHelper::IsLoaded(
                    selection.new_contents),
                /*show_reason_tab_switching=*/true,
                /*show_reason_unoccluded=*/false,
                /*show_reason_bfcache_restore=*/false);
      }
      tab_switch_event_latency_recorder_.OnWillChangeActiveTab(now);
    }
    TabStripModelChange change;
    auto visibility_tracker = InstallRenderWigetVisibilityTracker(selection);
    for (auto& observer : observers_)
      observer.OnTabStripModelChanged(this, change, selection);
  }

  return selection;
}

void TabStripModel::SelectRelativeTab(bool next, UserGestureDetails detail) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (contents_data_.empty())
    return;

  const int start_index = active_index();
  base::Optional<tab_groups::TabGroupId> start_group =
      GetTabGroupForTab(start_index);

  // Ensure the active tab is not in a collapsed group so the while loop can
  // fallback on activating the active tab.
  DCHECK(!start_group.has_value() || !IsGroupCollapsed(start_group.value()));
  const int delta = next ? 1 : -1;
  int index = (start_index + count() + delta) % count();
  base::Optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  while (group.has_value() && IsGroupCollapsed(group.value())) {
    index = (index + count() + delta) % count();
    group = GetTabGroupForTab(index);
  }
  ActivateTabAt(index, detail);
}

void TabStripModel::MoveTabRelative(bool forward) {
  const int offset = forward ? 1 : -1;

  // TODO: this needs to be updated for multi-selection.
  const int current_index = active_index();
  base::Optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(current_index);

  int target_index = std::max(std::min(current_index + offset, count() - 1), 0);
  base::Optional<tab_groups::TabGroupId> target_group =
      GetTabGroupForTab(target_index);

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  if (current_group != target_group) {
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
        target_index =
            forward ? tabs_in_group.end() - 1 : tabs_in_group.start();
      } else {
        GroupTab(current_index, target_group.value());
        return;
      }
    }
  }
  MoveWebContentsAt(current_index, target_index, true);
}

void TabStripModel::MoveWebContentsAtImpl(int index,
                                          int to_position,
                                          bool select_after_move) {
  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveWebContents(), selection_model_);

  std::unique_ptr<WebContentsData> moved_data =
      std::move(contents_data_[index]);
  WebContents* web_contents = moved_data->web_contents();
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
  for (auto& observer : observers_)
    observer.OnTabStripModelChanged(this, change, selection);
}

void TabStripModel::MoveSelectedTabsToImpl(int index,
                                           size_t start,
                                           size_t length) {
  DCHECK(start < selection_model_.selected_indices().size() &&
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
  DCHECK(!std::any_of(
      contents_data_.cbegin(), contents_data_.cend(),
      [new_group](const auto& datum) { return datum->group() == new_group; }));

  group_model_->AddTabGroup(new_group, base::nullopt);

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
    base::Optional<tab_groups::TabGroupId> destination_group =
        GetTabGroupForTab(destination_candidate);
    if (!destination_group.has_value() ||
        destination_group != GetTabGroupForTab(indices[0])) {
      destination_index = destination_candidate;
      break;
    }
  }

  MoveTabsAndSetGroupImpl(indices, destination_index, new_group);
}

void TabStripModel::AddToExistingGroupImpl(
    const std::vector<int>& indices,
    const tab_groups::TabGroupId& group) {
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
    base::Optional<tab_groups::TabGroupId> group) {
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
    base::Optional<tab_groups::TabGroupId> new_group) {
  if (new_group.has_value()) {
    // Unpin tabs when grouping -- the states should be mutually exclusive.
    // Here we manually unpin the tab to avoid moving the tab twice, which can
    // potentially cause race conditions.
    if (IsTabPinned(index)) {
      contents_data_[index]->set_pinned(false);
      for (auto& observer : observers_) {
        observer.TabPinnedStateChanged(
            this, contents_data_[index]->web_contents(), index);
      }
    }

    GroupTab(index, new_group.value());
  } else {
    UngroupTab(index);
  }

  if (index != new_index)
    MoveWebContentsAtImpl(index, new_index, false);
}

void TabStripModel::AddToReadLaterImpl(const std::vector<int>& indices) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  if (!model || !model->loaded())
    return;

  for (int index : indices) {
    WebContents* contents = GetWebContentsAt(index);
    chrome::MoveTabToReadLater(chrome::FindBrowserWithWebContents(contents),
                               contents);
  }
}

base::Optional<tab_groups::TabGroupId> TabStripModel::UngroupTab(int index) {
  base::Optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  if (!group.has_value())
    return base::nullopt;

  // Update the tab.
  contents_data_[index]->set_group(base::nullopt);
  for (auto& observer : observers_) {
    observer.TabGroupedStateChanged(
        base::nullopt, contents_data_[index]->web_contents(), index);
  }

  // Update the group model.
  TabGroup* tab_group = group_model_->GetTabGroup(group.value());
  tab_group->RemoveTab();
  if (tab_group->IsEmpty())
    group_model_->RemoveTabGroup(group.value());

  return group;
}

void TabStripModel::GroupTab(int index, const tab_groups::TabGroupId& group) {
  // Check for an old group first, so that any groups that are changed can be
  // notified appropriately.
  base::Optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
  if (old_group.has_value()) {
    if (old_group.value() == group)
      return;
    else
      UngroupTab(index);
  }
  contents_data_[index]->set_group(group);
  for (auto& observer : observers_) {
    observer.TabGroupedStateChanged(
        group, contents_data_[index]->web_contents(), index);
  }

  group_model_->GetTabGroup(group)->AddTab();
}

void TabStripModel::SetTabPinnedImpl(int index, bool pinned) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned() == pinned)
    return;

  // Upgroup tabs if pinning -- the states should be mutually exclusive.
  if (pinned)
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
    observer.TabPinnedStateChanged(this, contents_data_[index]->web_contents(),
                                   index);
  }
}

std::vector<int> TabStripModel::SetTabsPinned(const std::vector<int>& indices,
                                              bool pinned) {
  std::vector<int> new_indices;
  if (pinned) {
    for (size_t i = 0; i < indices.size(); i++) {
      if (IsTabPinned(indices[i])) {
        new_indices.push_back(indices[i]);
      } else {
        SetTabPinnedImpl(indices[i], true);
        new_indices.push_back(IndexOfFirstNonPinnedTab() - 1);
      }
    }
  } else {
    for (size_t i = indices.size() - 1; i < indices.size(); i--) {
      if (!IsTabPinned(indices[i])) {
        new_indices.push_back(indices[i]);
      } else {
        SetTabPinnedImpl(indices[i], false);
        new_indices.push_back(IndexOfFirstNonPinnedTab());
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
    if (url.SchemeIs(content::kChromeUIScheme)) {
      // chrome:// URLs don't have content settings but can be muted, so just
      // mute the WebContents.
      chrome::SetTabAudioMuted(web_contents, mute,
                               TabMutedReason::CONTENT_SETTING_CHROME,
                               std::string());
    } else {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      HostContentSettingsMap* settings =
          HostContentSettingsMapFactory::GetForProfile(profile);
      ContentSetting setting =
          mute ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;

      if (!profile->IsIncognitoProfile() &&
          setting == settings->GetDefaultContentSetting(
                         ContentSettingsType::SOUND, nullptr)) {
        setting = CONTENT_SETTING_DEFAULT;
      }
      settings->SetContentSettingDefaultScope(
          url, url, ContentSettingsType::SOUND, setting);
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
    data->set_opener(new_opener == data->web_contents() ? nullptr : new_opener);
  }

  // Sanity check that none of the tabs' openers refer |old_contents| or
  // themselves.
  DCHECK(!std::any_of(
      contents_data_.begin(), contents_data_.end(),
      [old_contents](const std::unique_ptr<WebContentsData>& data) {
        return data->opener() == old_contents ||
               data->opener() == data->web_contents();
      }));
}

void TabStripModel::EnsureGroupContiguity(int index) {
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
