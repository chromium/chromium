// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/feature_engagement/buildflags.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

bool DetermineTabStripLayoutStacked(PrefService* prefs, bool* adjust_layout) {
  *adjust_layout = false;
  // For ash, always allow entering stacked mode.
#if defined(OS_CHROMEOS)
  *adjust_layout = true;
  return prefs->GetBoolean(prefs::kTabStripStackedLayout);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceStackedTabStripLayout);
#endif
}

}  // namespace

class BrowserTabStripController::TabContextMenuContents
    : public ui::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(Tab* tab,
                         BrowserTabStripController* controller)
      : tab_(tab),
        controller_(controller),
        last_command_(TabStripModel::CommandFirst) {
    model_.reset(new TabMenuModel(
        this, controller->model_,
        controller->tabstrip_->GetModelIndexOfTab(tab)));
    menu_runner_.reset(new views::MenuRunner(
        model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  }

  ~TabContextMenuContents() override {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

  void Cancel() {
    controller_ = NULL;
  }

  void RunMenuAt(const gfx::Point& point, ui::MenuSourceType source_type) {
    menu_runner_->RunMenuAt(tab_->GetWidget(), NULL,
                            gfx::Rect(point, gfx::Size()),
                            views::MENU_ANCHOR_TOPLEFT, source_type);
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override {
    return controller_->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    int browser_cmd;
    return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                             &browser_cmd) ?
        controller_->tabstrip_->GetWidget()->GetAccelerator(browser_cmd,
                                                            accelerator) :
        false;
  }
  void CommandIdHighlighted(int command_id) override {
    controller_->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = static_cast<TabStripModel::ContextMenuCommand>(command_id);
    controller_->StartHighlightTabsForCommand(last_command_, tab_);
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    // Executing the command destroys |this|, and can also end up destroying
    // |controller_|. So stop the highlights before executing the command.
    controller_->tabstrip_->StopAllHighlighting();
    controller_->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

  void MenuClosed(ui::SimpleMenuModel* /*source*/) override {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

 private:
  std::unique_ptr<TabMenuModel> model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The tab we're showing a menu for.
  Tab* tab_;

  // A pointer back to our hosting controller, for command state information.
  BrowserTabStripController* controller_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_COPY_AND_ASSIGN(TabContextMenuContents);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(TabStripModel* model,
                                                     BrowserView* browser_view)
    : model_(model),
      tabstrip_(NULL),
      browser_view_(browser_view),
      hover_tab_selector_(model) {
  model_->AddObserver(this);

  local_pref_registrar_.Init(g_browser_process->local_state());
  local_pref_registrar_.Add(
      prefs::kTabStripStackedLayout,
      base::Bind(&BrowserTabStripController::UpdateStackedLayout,
                 base::Unretained(this)));
}

BrowserTabStripController::~BrowserTabStripController() {
  // When we get here the TabStrip is being deleted. We need to explicitly
  // cancel the menu, otherwise it may try to invoke something on the tabstrip
  // from its destructor.
  if (context_menu_contents_.get())
    context_menu_contents_->Cancel();

  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel(TabStrip* tabstrip) {
  tabstrip_ = tabstrip;

  UpdateStackedLayout();

  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i)
    AddTab(model_->GetWebContentsAt(i), i, model_->active_index() == i);
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) const {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  return model_->ContainsIndex(model_index) ?
      model_->IsContextMenuCommandEnabled(model_index, command_id) : false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  if (model_->ContainsIndex(model_index))
    model_->ExecuteContextMenuCommand(model_index, command_id);
}

bool BrowserTabStripController::IsTabPinned(Tab* tab) const {
  return IsTabPinned(tabstrip_->GetModelIndexOfTab(tab));
}

const ui::ListSelectionModel&
BrowserTabStripController::GetSelectionModel() const {
  return model_->selection_model();
}

int BrowserTabStripController::GetCount() const {
  return model_->count();
}

bool BrowserTabStripController::IsValidIndex(int index) const {
  return model_->ContainsIndex(index);
}

bool BrowserTabStripController::IsActiveTab(int model_index) const {
  return model_->active_index() == model_index;
}

int BrowserTabStripController::GetActiveIndex() const {
  return model_->active_index();
}

bool BrowserTabStripController::IsTabSelected(int model_index) const {
  return model_->IsTabSelected(model_index);
}

bool BrowserTabStripController::IsTabPinned(int model_index) const {
  return model_->ContainsIndex(model_index) && model_->IsTabPinned(model_index);
}

void BrowserTabStripController::SelectTab(int model_index) {
  model_->ActivateTabAt(model_index, true);
}

void BrowserTabStripController::ExtendSelectionTo(int model_index) {
  model_->ExtendSelectionTo(model_index);
}

void BrowserTabStripController::ToggleSelected(int model_index) {
  model_->ToggleSelectionAt(model_index);
}

void BrowserTabStripController::AddSelectionFromAnchorTo(int model_index) {
  model_->AddSelectionFromAnchorTo(model_index);
}

void BrowserTabStripController::CloseTab(int model_index,
                                         CloseTabSource source) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  model_->CloseWebContentsAt(model_index,
                             TabStripModel::CLOSE_USER_GESTURE |
                             TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void BrowserTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  context_menu_contents_.reset(new TabContextMenuContents(tab, this));
  context_menu_contents_->RunMenuAt(p, source_type);
}

int BrowserTabStripController::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions();
}

void BrowserTabStripController::OnDropIndexUpdate(int index,
                                                  bool drop_before) {
  // Perform a delayed tab transition if hovering directly over a tab.
  // Otherwise, cancel the pending one.
  if (index != -1 && !drop_before) {
    hover_tab_selector_.StartTabTransition(index);
  } else {
    hover_tab_selector_.CancelTabTransition();
  }
}

bool BrowserTabStripController::IsCompatibleWith(TabStrip* other) const {
  Profile* other_profile = other->controller()->GetProfile();
  return other_profile == GetProfile();
}

NewTabButtonPosition BrowserTabStripController::GetNewTabButtonPosition()
    const {
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNewTabButtonPosition);
  if (switch_value == switches::kNewTabButtonPositionOppositeCaption)
    return GetFrameView()->CaptionButtonsOnLeadingEdge() ? TRAILING : LEADING;
  if (switch_value == switches::kNewTabButtonPositionLeading)
    return LEADING;
  if (switch_value == switches::kNewTabButtonPositionAfterTabs)
    return AFTER_TABS;
  if (switch_value == switches::kNewTabButtonPositionTrailing)
    return TRAILING;

  return AFTER_TABS;
}

void BrowserTabStripController::CreateNewTab() {
  model_->delegate()->AddTabAt(GURL(), -1, true);
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  auto* new_tab_tracker =
      feature_engagement::NewTabTrackerFactory::GetInstance()->GetForProfile(
          browser_view_->browser()->profile());
  new_tab_tracker->OnNewTabOpened();
  new_tab_tracker->CloseBubble();
#endif
}

void BrowserTabStripController::CreateNewTabWithLocation(
    const base::string16& location) {
  // Use autocomplete to clean up the text, going so far as to turn it into
  // a search query if necessary.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())
      ->Classify(location, false, false, metrics::OmniboxEventProto::BLANK,
                 &match, NULL);
  if (match.destination_url.is_valid())
    model_->delegate()->AddTabAt(match.destination_url, -1, true);
}

void BrowserTabStripController::StackedLayoutMaybeChanged() {
  bool adjust_layout = false;
  bool stacked_layout = DetermineTabStripLayoutStacked(
      g_browser_process->local_state(), &adjust_layout);
  if (!adjust_layout || stacked_layout == tabstrip_->stacked_layout())
    return;

  g_browser_process->local_state()->SetBoolean(prefs::kTabStripStackedLayout,
                                               tabstrip_->stacked_layout());
}

bool BrowserTabStripController::IsSingleTabModeAvailable() {
  return GetFrameView()->IsSingleTabModeAvailable();
}

bool BrowserTabStripController::ShouldDrawStrokes() const {
  return GetFrameView()->ShouldDrawStrokes();
}

void BrowserTabStripController::OnStartedDraggingTabs() {
  if (!immersive_reveal_lock_.get()) {
    // The top-of-window views should be revealed while the user is dragging
    // tabs in immersive fullscreen. The top-of-window views may not be already
    // revealed if the user is attempting to attach a tab to a tabstrip
    // belonging to an immersive fullscreen window.
    immersive_reveal_lock_.reset(
        browser_view_->immersive_mode_controller()->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_NO));
  }
}

void BrowserTabStripController::OnStoppedDraggingTabs() {
  immersive_reveal_lock_.reset();
}

bool BrowserTabStripController::IsFrameCondensed() const {
  return GetFrameView()->IsFrameCondensed();
}

bool BrowserTabStripController::HasVisibleBackgroundTabShapes() const {
  return GetFrameView()->HasVisibleBackgroundTabShapes(
      BrowserNonClientFrameView::kUseCurrent);
}

bool BrowserTabStripController::EverHasVisibleBackgroundTabShapes() const {
  return GetFrameView()->EverHasVisibleBackgroundTabShapes();
}

SkColor BrowserTabStripController::GetFrameColor() const {
  return GetFrameView()->GetFrameColor();
}

SkColor BrowserTabStripController::GetToolbarTopSeparatorColor() const {
  return GetFrameView()->GetToolbarTopSeparatorColor();
}

SkColor BrowserTabStripController::GetTabBackgroundColor(TabState state) const {
  return GetFrameView()->GetTabBackgroundColor(state);
}

SkColor BrowserTabStripController::GetTabForegroundColor(TabState state) const {
  return GetFrameView()->GetTabForegroundColor(state);
}

int BrowserTabStripController::GetTabBackgroundResourceId(
    BrowserNonClientFrameView::ActiveState active_state,
    bool* has_custom_image) const {
  return GetFrameView()->GetTabBackgroundResourceId(active_state,
                                                    has_custom_image);
}

base::string16 BrowserTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return browser_view_->GetAccessibleTabLabel(
      false /* include_app_name */, tabstrip_->GetModelIndexOfTab(tab));
}

Profile* BrowserTabStripController::GetProfile() const {
  return model_->profile();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(TabStripModel* tab_strip_model,
                                              WebContents* contents,
                                              int model_index,
                                              bool is_active) {
  DCHECK(contents);
  DCHECK(model_->ContainsIndex(model_index));
  AddTab(contents, model_index, is_active);
}

void BrowserTabStripController::TabDetachedAt(WebContents* contents,
                                              int model_index,
                                              bool was_active) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->RemoveTabAt(contents, model_index, was_active);
}

void BrowserTabStripController::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  // It's possible for |new_contents| to be null when the final tab in a tab
  // strip is closed.
  if (new_contents && index != TabStripModel::kNoTab) {
    TabUIHelper::FromWebContents(new_contents)->set_was_active_at_least_once();
    SetTabDataAt(new_contents, index);
  }
}

void BrowserTabStripController::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  tabstrip_->SetSelection(model_->selection_model());
}

void BrowserTabStripController::TabMoved(WebContents* contents,
                                         int from_model_index,
                                         int to_model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  // A move may have resulted in the pinned state changing, so pass in a
  // TabRendererData.
  tabstrip_->MoveTab(
      from_model_index, to_model_index,
      TabRendererDataFromModel(contents, to_model_index, EXISTING_TAB));
}

void BrowserTabStripController::TabChangedAt(WebContents* contents,
                                             int model_index,
                                             TabChangeType change_type) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabReplacedAt(TabStripModel* tab_strip_model,
                                              WebContents* old_contents,
                                              WebContents* new_contents,
                                              int model_index) {
  SetTabDataAt(new_contents, model_index);
}

void BrowserTabStripController::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabBlockedStateChanged(WebContents* contents,
                                                       int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::SetTabNeedsAttentionAt(int index,
                                                       bool attention) {
  tabstrip_->SetTabNeedsAttention(index, attention);
}

BrowserNonClientFrameView* BrowserTabStripController::GetFrameView() {
  return browser_view_->frame()->GetFrameView();
}

const BrowserNonClientFrameView* BrowserTabStripController::GetFrameView()
    const {
  return browser_view_->frame()->GetFrameView();
}

TabRendererData BrowserTabStripController::TabRendererDataFromModel(
    WebContents* contents,
    int model_index,
    TabStatus tab_status) {
  TabRendererData data;
  TabUIHelper* tab_ui_helper = TabUIHelper::FromWebContents(contents);
  data.favicon = tab_ui_helper->GetFavicon().AsImageSkia();
  data.network_state = TabNetworkStateForWebContents(contents);
  data.title = tab_ui_helper->GetTitle();
  data.url = contents->GetURL();
  data.crashed_status = contents->GetCrashedStatus();
  data.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  data.pinned = model_->IsTabPinned(model_index);
  data.show_icon = data.pinned || favicon::ShouldDisplayFavicon(contents);
  data.blocked = model_->IsTabBlocked(model_index);
  data.alert_state = chrome::GetTabAlertStateForContents(contents);
  data.should_hide_throbber = tab_ui_helper->ShouldHideThrobber();
  return data;
}

void BrowserTabStripController::SetTabDataAt(content::WebContents* web_contents,
                                             int model_index) {
  tabstrip_->SetTabData(
      model_index,
      TabRendererDataFromModel(web_contents, model_index, EXISTING_TAB));
}

void BrowserTabStripController::StartHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseOtherTabs ||
      command_id == TabStripModel::CommandCloseTabsToRight) {
    int model_index = tabstrip_->GetModelIndexOfTab(tab);
    if (IsValidIndex(model_index)) {
      std::vector<int> indices =
          model_->GetIndicesClosedByCommand(model_index, command_id);
      for (std::vector<int>::const_iterator i(indices.begin());
           i != indices.end(); ++i) {
        tabstrip_->StartHighlight(*i);
      }
    }
  }
}

void BrowserTabStripController::StopHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseTabsToRight ||
      command_id == TabStripModel::CommandCloseOtherTabs) {
    // Just tell all Tabs to stop pulsing - it's safe.
    tabstrip_->StopAllHighlighting();
  }
}

void BrowserTabStripController::AddTab(WebContents* contents,
                                       int index,
                                       bool is_active) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->AddTabAt(index, TabRendererDataFromModel(contents, index, NEW_TAB),
                      is_active);
}

void BrowserTabStripController::UpdateStackedLayout() {
  bool adjust_layout = false;
  bool stacked_layout = DetermineTabStripLayoutStacked(
      g_browser_process->local_state(), &adjust_layout);
  tabstrip_->set_adjust_layout(adjust_layout);
  tabstrip_->SetStackedLayout(stacked_layout);
}
