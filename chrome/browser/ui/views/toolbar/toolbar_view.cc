// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/tab_search_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_tab_button.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/glic/glic_and_actor_buttons_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_divider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/webui_back_forward_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include <array>

#include "ui/aura/window_occlusion_tracker.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActionItemUnderlineIndicatorKey, false)

namespace {

// Gets the display mode for a given browser.
ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {
  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB
  // display mode.
  if (web_app::AppBrowserController::IsWebApp(browser)) {
    return ToolbarView::DisplayMode::kCustomTab;
  }

  if (browser->SupportsWindowFeature(
          Browser::WindowFeature::kFeatureTabStrip)) {
    return ToolbarView::DisplayMode::kNormal;
  }

  return ToolbarView::DisplayMode::kLocation;
}

auto& GetViewCommandMap() {
  static constexpr auto kViewCommandMap = base::MakeFixedFlatMap<int, int>(
      {{VIEW_ID_BACK_BUTTON, IDC_BACK},
       {VIEW_ID_FORWARD_BUTTON, IDC_FORWARD},
       {VIEW_ID_HOME_BUTTON, IDC_HOME},
       {VIEW_ID_RELOAD_BUTTON, IDC_RELOAD},
       {VIEW_ID_AVATAR_BUTTON, IDC_SHOW_AVATAR_MENU}});
  return kViewCommandMap;
}

constexpr int kBrowserAppMenuRefreshExpandedMargin = 5;
constexpr int kBrowserAppMenuRefreshCollapsedMargin = 2;
constexpr int kLargeSpaceBetweenButtons = 6;
constexpr int kInsideBorderAroundGlicButtons = 2;
constexpr int kOutsideBorderAroundGlicButtons = 11;
constexpr int kGlicButtonMargin = 5;

// Returns whether `point` should be treated as part of the caption area in
// `view`. Recursively traverses into icon containers to correctly handle
// padding between buttons.
bool IsPositionInWindowCaptionForView(const views::View* view,
                                      const gfx::Point& point) {
  for (const views::View* child : view->children()) {
    if (!child->GetVisible() || !child->bounds().Contains(point)) {
      continue;
    }
    // Recurse into known icon container types to check their children.
    if (views::IsViewClass<ToolbarIconContainerView>(child) ||
        views::IsViewClass<page_actions::PageActionContainerView>(child)) {
      const gfx::Point point_in_child =
          views::View::ConvertPointToTarget(view, child, point);
      return IsPositionInWindowCaptionForView(child, point_in_child);
    }
    // Separators and dividers are non-interactive and should be treated
    // as caption area.
    if (views::IsViewClass<views::Separator>(child) ||
        views::IsViewClass<ToolbarDivider>(child)) {
      return true;
    }
    // The point hit an interactive control (button, location bar, etc.).
    return false;
  }
  // The point is not in any child's bounds — it's in empty space between
  // children, padding, or above/below a child. In VTS mode the toolbar is
  // at the very top of the window, so all non-interactive areas should be
  // draggable regardless of vertical position.
  return true;
}

void SetRefreshMargins(views::View* button, bool expanded) {
  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, expanded ? kBrowserAppMenuRefreshExpandedMargin
                                  : kBrowserAppMenuRefreshCollapsedMargin));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolbarView, kToolbarElementId);

ToolbarView::ToolbarView(Browser* browser, BrowserView* browser_view)
    : AnimationDelegateViews(this),
      browser_(browser),
      browser_view_(browser_view),
      app_menu_icon_controller_(browser->profile(), this),
      display_mode_(GetDisplayMode(browser)) {
  // WebApp type-browsers set their own ToolbarButtonProvider.
  if (!web_app::AppBrowserController::IsWebApp(browser)) {
    scoped_unowned_user_data_.emplace(browser_->GetUnownedUserDataHost(),
                                      *this);
  }

  SetID(VIEW_ID_TOOLBAR);
  SetProperty(views::kElementIdentifierKey, kToolbarElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);

  if (display_mode_ == DisplayMode::kNormal) {
    for (const auto& view_and_command : GetViewCommandMap()) {
      chrome::AddCommandObserver(browser_, view_and_command.second, this);
    }
  }
  views::SetCascadingColorProviderColor(this, views::kCascadingBackgroundColor,
                                        kColorToolbar);

  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(this, gfx::Insets()), this);

  glic::GlicNudgeController* glic_nudge_controller =
      browser_->browser_window_features()->glic_nudge_controller();

  // `glic_nudge_controller` will be null if feature is not enabled.
  if (glic_nudge_controller) {
    glic_nudge_controller->SetToolbarDelegate(this);
  }
}

ToolbarView::~ToolbarView() {
  if (display_mode_ != DisplayMode::kNormal) {
    return;
  }

  overflow_button_->set_toolbar_controller(nullptr);

  for (const auto& view_and_command : GetViewCommandMap()) {
    chrome::RemoveCommandObserver(browser_, view_and_command.second, this);
  }

  glic::GlicNudgeController* glic_nudge_controller =
      browser_->browser_window_features()->glic_nudge_controller();
  if (glic_nudge_controller) {
    glic_nudge_controller->SetToolbarDelegate(/*delegate=*/nullptr);
  }
}

void ToolbarView::Init() {
#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/40171404#comment3
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif

  std::unique_ptr<LocationBarView> location_bar_view;
  std::unique_ptr<WebUILocationBar> webui_location_bar;
  if (features::IsWebUILocationBarEnabled() &&
      /* TODO(http://crbug.com/470042732): Figure out where we fit in other
       * modes. When doing this, we have to be careful of floating DevTools ---
       * that secretly has a hidden toolbar in location mode.*/
      display_mode_ == DisplayMode::kNormal) {
    webui_location_bar = std::make_unique<WebUILocationBar>(browser_, this);
  } else {
    location_bar_view = std::make_unique<LocationBarView>(
        browser_, browser_->profile(), browser_->command_controller(), this,
        display_mode_ != DisplayMode::kNormal);
  }

  // Make sure the toolbar shows by default.
  size_animation_.Reset(1);

  if (display_mode_ != DisplayMode::kNormal) {
    CHECK(location_bar_view)
        << "Alternate location bar impls need to handle this.";
    location_bar_view_ = AddChildView(std::move(location_bar_view));
    location_bar_ = location_bar_view_;
    location_bar_view_->Init();
  }

  if (display_mode_ == DisplayMode::kNormal) {
    SetBackground(std::make_unique<CustomCornersBackground>(
        *this, *browser_view_,
        /*primary_color=*/CustomCornersBackground::ToolbarTheme(),
        /*corner_color=*/CustomCornersBackground::FrameTheme()));
  } else if (display_mode_ == DisplayMode::kCustomTab) {
    custom_tab_bar_ =
        AddChildView(std::make_unique<CustomTabBarView>(browser_view_, this));
    SetLayoutManager(std::make_unique<views::FillLayout>());
    initialized_ = true;
    return;
  } else if (display_mode_ == DisplayMode::kLocation) {
    // Add the pinned toolbar actions container so that downloads can be shown
    // in popups.
    pinned_toolbar_actions_container_ = AddChildView(
        std::make_unique<PinnedToolbarActionsContainer>(browser_view_, this));
    pinned_toolbar_actions_ = pinned_toolbar_actions_container_;
    SetBackground(views::CreateSolidBackground(kColorLocationBarBackground));
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero))
        .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);
    if (location_bar_view_) {
      location_bar_view_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));
    }
    initialized_ = true;
    return;
  }

  const auto callback = [](Browser* browser, int command,
                           const ui::Event& event) {
    chrome::ExecuteCommandWithDisposition(
        browser, command, ui::DispositionFromEventFlags(event.flags()));
  };

  PrefService* const prefs = browser_->profile()->GetPrefs();

  std::unique_ptr<ExtensionsToolbarDesktop> extensions_container;
  std::unique_ptr<ToolbarDivider> toolbar_divider;

  // Do not create the extensions or browser actions container if it is a guest
  // profile (only regular and incognito profiles host extensions).
  if (!browser_->profile()->IsGuestSession() &&
      !features::IsWebUIExtensionsContainerEnabled()) {
    extensions_container = std::make_unique<ExtensionsToolbarDesktop>(browser_);

    toolbar_divider = std::make_unique<ToolbarDivider>();
  }

  std::unique_ptr<MediaToolbarButtonView> media_button;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  media_button = std::make_unique<MediaToolbarButtonView>(
      browser_view_,
      std::make_unique<MediaToolbarButtonContextualMenu>(browser_));
#endif

  // Always add children in order from left to right, for accessibility.
  if (!features::IsWebUIBackForwardButtonEnabled()) {
    back_ = AddChildView(std::make_unique<BackForwardButton>(
        BackForwardButton::Direction::kBack,
        base::BindRepeating(callback, browser_, IDC_BACK), browser_));
    forward_ = AddChildView(std::make_unique<BackForwardButton>(
        BackForwardButton::Direction::kForward,
        base::BindRepeating(callback, browser_, IDC_FORWARD), browser_));
  }

  if (base::FeatureList::IsEnabled(
          features::kWebUIToolbarProcessOverheadExperiment)) {
    detached_toolbar_webview_ = std::make_unique<WebUIToolbarWebView>(
        browser_, browser_->command_controller(), /*location_bar=*/nullptr);
  } else if (features::IsWebUIToolbarEnabled()) {
    toolbar_webview_ = AddChildView(std::make_unique<WebUIToolbarWebView>(
        browser_, browser_->command_controller(),
        std::move(webui_location_bar)));

    toolbar_webview_->SetProperty(views::kFlexBehaviorKey,
                                  toolbar_webview_->GetFlexSpecification());
  }

  if (!features::IsWebUIReloadButtonEnabled() ||
      base::FeatureList::IsEnabled(
          features::kWebUIToolbarProcessOverheadExperiment)) {
    reload_ = AddChildView(std::make_unique<ReloadButton>(
        browser_->profile(), browser_->command_controller(),
        InitialWebUIWindowMetricsManager::From(browser_)));
  }

  if (!features::IsWebUIHomeButtonEnabled()) {
    home_ = AddChildView(std::make_unique<HomeButton>(
        browser_, base::BindRepeating(callback, browser_, IDC_HOME)));
  }

  if (!features::IsWebUISplitTabsButtonEnabled()) {
    split_tabs_ =
        AddChildView(std::make_unique<SplitTabsToolbarButton>(browser_));
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
      contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    auto button = std::make_unique<ContextualTasksButton>(browser_);
    auto* vts_controller =
        tabs::VerticalTabStripStateController::From(browser_);
    if (!vts_controller || !vts_controller->ShouldDisplayVerticalTabs()) {
      button->SetProperty(views::kMarginsKey, gfx::Insets());
    }
    AddChildViewAt(std::move(button), 0);
  }

  if (location_bar_view) {
    location_bar_view_ = AddChildView(std::move(location_bar_view));
    location_bar_ = location_bar_view_;
  } else {
    location_bar_ = toolbar_webview_->GetLocationBar();
  }

  bool is_glic_left_of_profile =
      features::kGlicToolbarButtonLocationParam.Get() ==
          features::GlicToolbarButtonLocation::kLeftOfProfileChip ||
      features::kGlicToolbarButtonLocationParam.Get() ==
          features::GlicToolbarButtonLocation::kLeftOfProfileChipWithBackground;
  if (glic::GlicEnabling::IsProfileEligible(browser_view_->GetProfile()) &&
      !is_glic_left_of_profile) {
    InitGlicContainer();

    glic_button_ = AddChildView(CreateGlicButton());
    std::unique_ptr<ToolbarDivider> glic_button_divider =
        std::make_unique<ToolbarDivider>();
    glic_button_divider_ = AddChildView(std::move(glic_button_divider));
    glic_button_divider_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(
            0, GetLayoutConstant(LayoutConstant::kToolbarDividerSpacing)));
  }

  if (extensions_container) {
    extensions_container_ = AddChildView(std::move(extensions_container));
    extensions_toolbar_coordinator_ =
        std::make_unique<ExtensionsToolbarCoordinator>(browser_,
                                                       extensions_container_);
  }

  if (toolbar_divider) {
    toolbar_divider_ = AddChildView(std::move(toolbar_divider));
  }

  if (!features::IsWebUIPinnedToolbarActionsEnabled()) {
    pinned_toolbar_actions_container_ = AddChildView(
        std::make_unique<PinnedToolbarActionsContainer>(browser_view_, this));
    pinned_toolbar_actions_ = pinned_toolbar_actions_container_;
  } else {
    pinned_toolbar_actions_ = toolbar_webview_->GetPinnedToolbarActions();
  }

  if (!base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton) &&
      features::HasTabSearchToolbarButton()) {
    CHECK(!features::IsWebUIPinnedToolbarActionsEnabled())
        << "WebUIPinnedToolbarActions does not support "
           "CreatePermanentButtonFor, consider enabling "
           "HorizontalTabStripComboButton";
    tab_search_button_ =
        pinned_toolbar_actions_container_->CreatePermanentButtonFor(
            kActionTabSearch);
    tab_search_button_->SetProperty(views::kElementIdentifierKey,
                                    kTabSearchButtonElementId);
  }

  if (IsChromeLabsEnabled()) {
    UpdateChromeLabsNewBadgePrefs(browser_->profile());

    const bool should_show_chrome_labs_ui =
        ShouldShowChromeLabsUI(browser_->profile());
    if (should_show_chrome_labs_ui) {
      show_chrome_labs_button_.Init(
          chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, prefs,
          base::BindRepeating(&ToolbarView::OnChromeLabsPrefChanged,
                              base::Unretained(this)));
      CHECK(!features::IsWebUIPinnedToolbarActionsEnabled())
          << "WebUIPinnedToolbarActions does not support ChromeLabs.";
      // Set the visibility for the button based on initial enterprise policy
      // value. Only call OnChromeLabsPrefChanged if there is a change from
      // the initial value.
      pinned_toolbar_actions_container_->GetActionItemFor(kActionShowChromeLabs)
          ->SetVisible(show_chrome_labs_button_.GetValue() &&
                       should_show_chrome_labs_ui);
    }
  }

  // Only show the Battery Saver button when it is not controlled by the OS. On
  // ChromeOS the battery icon in the shelf shows the same information.
  if (!performance_manager::user_tuning::IsBatterySaverModeManagedByOS() &&
      !features::IsWebUIBatterySaverButtonEnabled()) {
    battery_saver_button_ =
        AddChildView(std::make_unique<BatterySaverButton>(browser_view_));
  }

  if (!features::IsWebUIPerformanceInterventionButtonEnabled()) {
    performance_intervention_button_ = AddChildView(
        std::make_unique<PerformanceInterventionButton>(browser_view_));
  }

  if (media_button) {
    media_button_ = AddChildView(std::move(media_button));
  }

  if (glic::GlicEnabling::IsProfileEligible(browser_view_->GetProfile())) {
    if (base::FeatureList::IsEnabled(features::kAiOverlayDialog) &&
        ttc::AiOverlayDialogController::From(browser_)) {
      actions::ActionItem* action_item =
          actions::ActionManager::Get().FindAction(
              kActionShowAiOverlayDialog, browser_->browser_window_features()
                                              ->browser_actions()
                                              ->root_action_item());
      if (action_item) {
        action_item->SetVisible(true);
        action_item->SetEnabled(true);
        PinnedToolbarActionsModel::Get(browser_->profile())
            ->UpdatePinnedState(kActionShowAiOverlayDialog, true);
      }
    }
  }

  if (is_glic_left_of_profile &&
      glic::GlicEnabling::IsProfileEligible(browser_view_->GetProfile())) {
    InitGlicContainer();

    glic_button_ = AddChildView(CreateGlicButton());
    // The left margin is needed to ensure proper spacing before the
    // separator. The right margin is needed for spacing between the glic and
    // actor icons. The space between glic and profile should also be 5 but that
    // is handled by the profile margins.
    glic_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets()
                                  .set_left(kGlicButtonMargin)
                                  .set_right(kInsideBorderAroundGlicButtons));
    UpdateGlicButtonVisibility();
  }

  if (!features::IsWebUIAvatarButtonEnabled()) {
    avatar_ =
        AddChildView(std::make_unique<AvatarToolbarButton>(browser_view_));
    bool show_avatar_toolbar_button = true;
#if BUILDFLAG(IS_CHROMEOS)
    // ChromeOS only badges Incognito, Guest, and captive portal signin icons in
    // the browser window.
    show_avatar_toolbar_button =
        browser_->profile()->IsIncognitoProfile() ||
        browser_->profile()->IsGuestSession() ||
        (browser_->profile()->IsOffTheRecord() &&
         browser_->profile()->GetOTRProfileID().IsCaptivePortal());
#else
    // DevTools profiles are OffTheRecord, so hide it there.
    show_avatar_toolbar_button = browser_->profile()->IsIncognitoProfile() ||
                                 browser_->profile()->IsGuestSession() ||
                                 browser_->profile()->IsRegularProfile();
#endif
    avatar_->SetVisible(show_avatar_toolbar_button);
  }

  overflow_button_ = AddChildView(std::make_unique<OverflowButton>());
  overflow_button_->SetVisible(false);

  // WebUI app menu button handles these internally, so no need to set these
  // properties here, and the control is added as part of the WebUI toolbar.
  if (!features::IsWebUIAppMenuButtonEnabled()) {
    auto app_menu_button = std::make_unique<BrowserAppMenuButton>(this);
    app_menu_button->SetFlipCanvasOnPaintForRTLUI(true);
    app_menu_button->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
    app_menu_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
    app_menu_button->SetID(VIEW_ID_APP_MENU);
    app_menu_button_ = AddChildView(std::move(app_menu_button));
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
      contextual_tasks::GetExpandButtonOption() ==
          contextual_tasks::ExpandButtonOption::kToolbarCloseButton) {
    AddChildView(std::make_unique<ContextualTasksCloseTabButton>(browser_));
  }

  LoadImages();

  // Set the button icon based on the system state. Do this after
  // |app_menu_button_| has been added as a bubble may be shown that needs
  // the widget (widget found by way of app_menu_button_->GetWidget()).
  app_menu_icon_controller_.UpdateDelegate();

  if (location_bar_view_) {
    location_bar_view_->Init();
  } else {
    toolbar_webview_->GetLocationBar()->Init(toolbar_webview_.get());
  }

  show_forward_button_.Init(
      prefs::kShowForwardButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowForwardButtonChanged,
                          base::Unretained(this)));

  SetForwardButtonVisibility(show_forward_button_.GetValue());

  show_home_button_.Init(
      prefs::kShowHomeButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowHomeButtonChanged,
                          base::Unretained(this)));

  if (home_) {
    home_->SetVisible(show_home_button_.GetValue());
  }

  if (glic::GlicEnabling::IsProfileEligible(browser_view_->GetProfile())) {
    auto* vertical_tab_strip_state_controller =
        tabs::VerticalTabStripStateController::From(browser_view_->browser());
    if (vertical_tab_strip_state_controller) {
      vertical_tab_subscription_ =
          vertical_tab_strip_state_controller->RegisterOnModeChanged(
              base::BindRepeating(&ToolbarView::OnVerticalTabStripModeChanged,
                                  base::Unretained(this)));
      should_display_vertical_tabs_ =
          vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs();
    }
    UpdateGlicButtonVisibility();
  }

  InitLayout();

  for (auto* button : std::array<views::Button*, 5>{back_, forward_, reload_,
                                                    home_, avatar_}) {
    if (button) {
      button->set_tag(GetViewCommandMap().at(button->GetID()));
    }
  }

  initialized_ = true;
}

void ToolbarView::InitGlicContainer() {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiTaskIcon.Get()) {
    glic_actor_button_container_ =
        AddChildView(CreateGlicActorButtonContainer());
    glic_actor_task_icon_ =
        glic_actor_button_container_->AddChildView(CreateGlicActorTaskIcon());
    glic_actor_button_container_->SetVisible(false);
    glic_actor_task_icon_->SetVisible(false);
  }
}

void ToolbarView::OnVerticalTabStripModeChanged(
    tabs::VerticalTabStripStateController* controller) {
  should_display_vertical_tabs_ = controller->ShouldDisplayVerticalTabs();
  UpdateGlicButtonVisibility();
  UpdateGlicActorVisibility();
}

std::unique_ptr<GlicAndActorButtonsContainer>
ToolbarView::CreateGlicActorButtonContainer() {
  auto glic_actor_button_container =
      std::make_unique<GlicAndActorButtonsContainer>();

  // Should be hidden until a task starts.
  glic_actor_button_container->SetVisible(false);

  return glic_actor_button_container;
}

std::unique_ptr<glic::ToolbarGlicActorTaskIcon>
ToolbarView::CreateGlicActorTaskIcon() {
  std::unique_ptr<glic::ToolbarGlicActorTaskIcon> glic_actor_task_icon =
      std::make_unique<glic::ToolbarGlicActorTaskIcon>(
          browser_view_->browser(),
          base::BindRepeating(&ToolbarView::OnGlicActorTaskIconClicked,
                              base::Unretained(this)));

  // Add a MenuButtonController in order to keep the task icon pressed while the
  // bubble is visible.
  glic_actor_task_icon->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          glic_actor_task_icon.get(),
          base::BindRepeating(&ToolbarView::OnGlicActorTaskIconClicked,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              glic_actor_task_icon.get())));

  glic_actor_task_icon->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kCenter);

  if (base::FeatureList::IsEnabled(features::kToolbarGlicButtonResizing)) {
    glic_actor_task_icon->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
            views::MaximumFlexSizeRule::kPreferred));
  }

  return glic_actor_task_icon;
}

void ToolbarView::OnGlicActorTaskIconClicked() {
  Profile* const profile = browser_view_->GetProfile();
  auto* icon_manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  CHECK(icon_manager);

  ActorTaskListBubbleController* controller =
      ActorTaskListBubbleController::From(browser_view_->browser());
  // Only show the bubble if the button is not currently pressed. Clicking on
  // the pressed button should dismiss the nudge.
  if (!glic_actor_task_icon_->GetIsPressed()) {
    controller->ShowBubble(glic_actor_task_icon_);
  }

  auto current_task_nudge_state = icon_manager->GetCurrentActorTaskNudgeState();
  actor::ui::LogGlobalTaskIndicatorClick(current_task_nudge_state);
}

std::unique_ptr<glic::ToolbarGlicButton> ToolbarView::CreateGlicButton() {
  glic::GlicKeyedService* service =
      glic::GlicKeyedService::Get(browser_view_->GetProfile());
  std::u16string tooltip_text = l10n_util::GetStringUTF16(
      service->instance_coordinator().IsAnyPanelShowing()
          ? IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE
          : IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);
  std::unique_ptr<glic::ToolbarGlicButton> glic_button =
      std::make_unique<glic::ToolbarGlicButton>(
          browser_view_->browser(),
          base::BindRepeating(&ToolbarView::OnGlicButtonAnimationEnded,
                              base::Unretained(this)),
          tooltip_text,
          base::BindRepeating(&ToolbarView::OnGlicButtonClicked,
                              base::Unretained(this)));

  glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kCenter);

  return glic_button;
}

void ToolbarView::OnGlicButtonClicked() {
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  BrowserUserEducationInterface::From(browser_)->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHGlicPromoFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

  std::optional<std::string> prompt_suggestion;
  glic::GlicNudgeController* glic_nudge_controller =
      browser_->browser_window_features()->glic_nudge_controller();
  if (glic_nudge_controller) {
    prompt_suggestion = glic_nudge_controller->GetPromptSuggestion();
    glic_nudge_controller->ClearPromptSuggestion();
  }

  glic::mojom::InvocationSource source;
  if (button_controller_) {
    source = button_controller_->GetInvocationSource(
        glic_button_->GetIsShowingNudge(), /*is_toolbar=*/true);
  } else {
    source = glic_button_->GetIsShowingNudge()
                 ? glic::mojom::InvocationSource::kNudge
                 : glic::mojom::InvocationSource::kToolbarButton;
  }

  auto* glic_service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser_view_->GetProfile());
  const bool is_panel_showing =
      glic_service->IsPanelShowingForBrowser(*browser_view_->browser());
  if (!is_panel_showing && prompt_suggestion.has_value() &&
      !prompt_suggestion->empty()) {
    glic::GlicInvokeOptions options(glic::Target(browser()), source);
    options.prompts.push_back(std::move(*prompt_suggestion));
    glic_service->Invoke(std::move(options));
  } else {
    glic_service->ToggleUI(browser_view_->browser(),
                           /*prevent_close=*/false, source);
  }

  if (glic_button_->GetIsShowingNudge()) {
    glic_nudge_controller->OnNudgeActivity(
        glic::GlicNudgeActivity::kNudgeClicked);
  }

  ExecuteHideToolbarNudge(glic_button_);
  // Reset state manually since there wont be a mouse up event as the
  // animation moves the button out of the way.
  glic_button_->SetState(views::Button::ButtonState::STATE_NORMAL);
}

void ToolbarView::OnGlicButtonDismissed() {
  browser_->browser_window_features()->glic_nudge_controller()->OnNudgeActivity(
      glic::GlicNudgeActivity::kNudgeDismissed);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideToolbarNudge(glic_button_);
}

void ToolbarView::OnGlicButtonAnimationEnded() {
  // TODO(crbug.com/484389669): ToolbarGlicButton animations
  return;
}

void ToolbarView::ShowToolbarNudge(glic::GlicButtonInterface* button) {
  if (IsMouseHovered()) {
    SetLockedExpansionMode(ExpansionMode::kWillShow, button);
    return;
  }
  if (locked_expansion_mode_ == ExpansionMode::kNone) {
    ExecuteShowToolbarNudge(button);
  }
}

void ToolbarView::HideToolbarNudge(glic::GlicButtonInterface* button) {
  if (this->IsMouseHovered()) {
    SetLockedExpansionMode(ExpansionMode::kWillHide, button);
    return;
  }
  if (locked_expansion_mode_ == ExpansionMode::kNone) {
    ExecuteHideToolbarNudge(button);
  }
}

bool ToolbarView::GetIsShowingGlicNudge() {
  return glic_button_ && glic_button_->GetIsShowingNudge();
}

bool ToolbarView::GetIsShowingGlicActorTaskIconNudge() {
  return glic_actor_task_icon_ && glic_actor_task_icon_->GetIsShowingNudge();
}

void ToolbarView::OnTriggerGlicNudgeUI(glic::NudgeParams params) {
  if (GetIsShowingGlicActorTaskIconNudge()) {
    return;
  }

  CHECK(glic_button_);
  if (!params.label.empty()) {
    glic_button_->SetNudgeLabel(std::move(params.label));
    ShowToolbarNudge(glic_button_);
  }
}

void ToolbarView::OnHideGlicNudgeUI() {
  if (glic_button_) {
    HideToolbarNudge(glic_button_);
  }
}

void ToolbarView::SetGlicActorNudgeLabel(const std::u16string& nudge_label) {
  glic_actor_task_icon()->ShowNudgeLabel(nudge_label);
}

void ToolbarView::TriggerGlicActorNudge(const std::u16string& nudge_text) {
  CHECK(glic_actor_task_icon_);
  if (GetIsShowingGlicNudge()) {
    // If the glic button is showing, start the hide animation in parallel to
    // the show actor nudge animation.
    HideToolbarNudge(glic_button_);
    OnGlicButtonAnimationEnded();
  }
  ShowGlicActorNudge(nudge_text);
}

bool ToolbarView::IsGlicAdded() {
  return glic_button_ && glic_actor_task_icon_;
}

void ToolbarView::ShowGlicActorNudge(const std::u16string nudge_text) {
  CHECK(glic_actor_task_icon_);
  // Start animation for minimizing the glic button.
  glic_button_->Collapse();
  ShowGlicActorTaskIcon();
  glic_actor_task_icon_->ShowNudgeLabel(nudge_text);
  ShowToolbarNudge(glic_actor_task_icon_);
}

void ToolbarView::ShowGlicActorTaskIcon() {
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  // If the nudge is showing (ex: previous state was CheckTasks), hide the
  // nudge.
  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    HideToolbarNudge(glic_actor_task_icon_);
    return;
  }
  glic_button_ =
      glic_actor_button_container_->InsertGlicButton(glic_button_.get());
  SetGlicActorShowState(true);
  SetGlicShowState(true);
  glic_button_->Collapse();
  glic_button_->SetSplitButtonCornerStyling();
  UpdateGlicActorButtonContainerBorders();

  if (glic_actor_task_icon_->GetAnimationMode() ==
      glic::AnimationMode::kEntry) {
    // TODO(crbug.com/484389669): Create animation session to being animation of
    // nudge.
    glic_actor_task_icon_->SetAnimationMode(glic::AnimationMode::kNudge);
    glic_actor_task_icon_->SetWidthFactor(1.0);
  }
}

void ToolbarView::HideGlicActorTaskIcon() {
  CHECK(glic_actor_task_icon_);

  // If it's already hidden, do nothing.
  if (!glic_actor_task_icon_->GetVisible() &&
      !glic_actor_task_icon_->GetIsShowingNudge()) {
    return;
  }
  glic_actor_task_icon_->SetIsShowingNudge(false);

  // TODO(crbug.com/484389669): Toolbar glic actor animations
  if (glic_actor_task_icon_->GetAnimationMode() ==
      glic::AnimationMode::kNudge) {
    // TODO(crbug.com/484389669): Create animation session to being animation of
    // nudge.
    glic_actor_task_icon_->SetAnimationMode(glic::AnimationMode::kEntry);
    glic_actor_task_icon_->SetWidthFactor(0.0);
  }

  FinalizeHideGlicActorTaskIcon();
}

void ToolbarView::SetGlicActorNudgePressedState(bool pressed) {
  glic_actor_task_icon()->SetPressedState(pressed);
}

void ToolbarView::ShowActorTaskListBubble() {
  ActorTaskListBubbleController::From(browser_)->ShowBubble(
      glic_actor_task_icon());
}

void ToolbarView::FinalizeHideGlicActorTaskIcon() {
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  // Reset Nudge State
  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    // TODO(crbug.com/484389669): Glic actor nudge animation
    glic_actor_task_icon_->SetIsShowingNudge(false);
  }
  glic_actor_task_icon_->SetVisible(false);
  glic_actor_task_icon_->SetTaskIconToDefault();

  size_t insertion_index = GetIndexOf(avatar_.get()).value();
  if (glic_button_divider_) {
    insertion_index = GetIndexOf(glic_button_divider_).value();
  }
  glic_button_ = AddChildViewAt(std::move(glic_button_.get()), insertion_index);
  glic_actor_button_container_->SetVisible(false);
  glic_button_->Expand();
  glic_button_->ResetSplitButtonCornerStyling();
  // Reset the animation mode for the next time the icon is shown.
  glic_actor_task_icon_->SetAnimationMode(glic::AnimationMode::kEntry);
  UpdateGlicActorButtonContainerBorders();
}

void ToolbarView::UpdateGlicActorButtonContainerBorders() {
  CHECK(glic_button_);
  gfx::Insets glic_border;

  // Ensure buttons look vertically centered by making the top and bottom insets
  // match.
  gfx::Insets border_insets = gfx::Insets();
  int min_vertical_inset =
      std::min(border_insets.top(), border_insets.bottom());
  border_insets.set_top_bottom(min_vertical_inset, min_vertical_inset);

  // GlicActorTaskIcon will only ever be shown alongside the GlicButton.
  if (glic_actor_task_icon_ && glic_actor_task_icon_->IsDrawn()) {
    gfx::Insets task_icon_border;
    const gfx::Insets right_icon_border =
        gfx::Insets().set_left_right(0, kOutsideBorderAroundGlicButtons);
    const gfx::Insets left_icon_border = gfx::Insets().set_left_right(
        kOutsideBorderAroundGlicButtons, kInsideBorderAroundGlicButtons);
    task_icon_border = right_icon_border + border_insets;
    glic_border = left_icon_border + border_insets;
    glic_actor_task_icon_->SetBorder(
        views::CreateEmptyBorder(task_icon_border));
    // Force a background repaint to account for the new border insets.
    glic_actor_task_icon_->RefreshBackground();
  } else {
    // Reset GlicButton border if Task Icon is hidden.
    glic_border = gfx::Insets().set_left_right(border_insets.top(),
                                               border_insets.bottom()) +
                  border_insets;
  }
  glic_button_->SetBorder(views::CreateEmptyBorder(glic_border));
  // Force a background repaint to account for the new border insets.
  glic_button_->RefreshBackground();
}

void ToolbarView::ExecuteShowToolbarNudge(glic::GlicButtonInterface* button) {
  // TODO(crbug.com/): Fix cases where we can't show modal ui during animation
  // session.
  button->SetIsShowingNudge(true);

  // Only change the margins between the GlicButton and nudges that are NOT
  // coming from the GlicActorTaskIcon.
  if (glic_button_ && glic_button_->GetVisible() && button != glic_button_ &&
      button != glic_actor_task_icon_) {
    const int space_between_buttons = kLargeSpaceBetweenButtons;
    gfx::Insets margin;
    margin.set_right(space_between_buttons);
    button->GetPropertyHandler()->SetProperty(views::kMarginsKey, margin);
  } else {
    // Reset the margins.
    button->GetPropertyHandler()->SetProperty(views::kMarginsKey,
                                              gfx::Insets());
  }
}

void ToolbarView::ExecuteHideToolbarNudge(glic::GlicButtonInterface* button) {
  if (!button->GetVisible()) {
    return;
  }

  // Since the glic button is still visible in it's hidden state we need to have
  // a special case to query if it's in its Hide state.
  if (button == glic_button_ && button->GetWidthFactor() == 0.0) {
    return;
  }

  button->SetIsShowingNudge(false);
}

void ToolbarView::UpdateGlicActorVisibility() {
  if (!glic_actor_task_icon_) {
    return;
  }

  bool is_glic_actor_visible =
      should_show_glic_actor_ &&
      (should_display_vertical_tabs_ ||
       base::FeatureList::IsEnabled(features::kGlicHorizontalTabToolbarButton));

  glic_actor_task_icon_->SetVisible(is_glic_actor_visible);
  if (glic_button_) {
    bool is_glic_left_of_profile =
        base::FeatureList::IsEnabled(features::kGlicToolbarButtonLocation) &&
        features::kGlicToolbarButtonLocationParam.Get() ==
            features::GlicToolbarButtonLocation::kLeftOfProfileChip;
    glic_button_->UpdateStyle(is_glic_left_of_profile &&
                              !is_glic_actor_visible);
  }
}

void ToolbarView::UpdateGlicButtonVisibility() {
  if (!glic_button_) {
    return;
  }

  bool is_glic_visible =
      should_show_glic_button_ &&
      (should_display_vertical_tabs_ ||
       base::FeatureList::IsEnabled(features::kGlicHorizontalTabToolbarButton));

  glic_button_->SetVisible(is_glic_visible);
  if (glic_button_divider_) {
    glic_button_divider_->SetVisible(is_glic_visible);
  }

  if (glic_actor_button_container_) {
    // glic_actor_button_container_ should only be visible at the same time as
    // glic_button_.
    glic_actor_button_container_->SetVisible(is_glic_visible);
  }
  bool is_glic_left_of_profile =
      base::FeatureList::IsEnabled(features::kGlicToolbarButtonLocation) &&
      features::kGlicToolbarButtonLocationParam.Get() ==
          features::GlicToolbarButtonLocation::kLeftOfProfileChip;
  bool is_task_icon_visible =
      glic_actor_task_icon_ && glic_actor_task_icon_->GetVisible();
  glic_button_->UpdateStyle(is_glic_left_of_profile && !is_task_icon_visible);
}

void ToolbarView::SetGlicActorShowState(bool show) {
  should_show_glic_actor_ = show;
  UpdateGlicActorVisibility();
}

void ToolbarView::SetButtonController(glic::GlicButtonController* controller) {
  button_controller_ = controller;
}

void ToolbarView::SetGlicShowState(bool show) {
  should_show_glic_button_ = show;
  UpdateGlicButtonVisibility();
}

void ToolbarView::SetGlicPanelIsOpen(bool open) {
  if (!glic_button_) {
    return;
  }

  glic_button_->SetGlicPanelIsOpen(open);
}

void ToolbarView::MouseMovedOutOfHost() {
  SetLockedExpansionMode(ExpansionMode::kNone, /*button=*/nullptr);
}

void ToolbarView::SetLockedExpansionMode(ExpansionMode mode,
                                         glic::GlicButtonInterface* button) {
  if (mode == ExpansionMode::kNone) {
    if (locked_expansion_mode_ == ExpansionMode::kWillShow) {
      ExecuteShowToolbarNudge(locked_expansion_button_);
    } else if (locked_expansion_mode_ == ExpansionMode::kWillHide) {
      ExecuteHideToolbarNudge(locked_expansion_button_);
    }
    locked_expansion_button_ = nullptr;
  } else {
    locked_expansion_button_ = button;
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void ToolbarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0) {
    SetToolbarVisibility(false);
  }
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/false);
}

void ToolbarView::AnimationProgressed(const gfx::Animation* animation) {
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/true);
}

void ToolbarView::Update(WebContents* tab) {
  if (location_bar_) {
    location_bar_->Update(tab);
  }

  if (extensions_container_) {
    extensions_container_->UpdateAllIcons();
  }

  if (pinned_toolbar_actions_container_) {
    pinned_toolbar_actions_container_->UpdateAllIcons();
  }

  if (ReloadControl* reload_control = GetReloadButton(); reload_control) {
    reload_control->SetDevToolsStatus(
        chrome::IsDebuggerAttachedToCurrentTab(browser_));
  }
}

bool ToolbarView::UpdateSecurityState() {
  if (location_bar_ && location_bar_->HasSecurityStateChanged()) {
    Update(nullptr);
    return true;
  }

  return false;
}

void ToolbarView::SetToolbarVisibility(bool visible) {
  SetVisible(visible);
  views::View* bar = display_mode_ == DisplayMode::kCustomTab
                         ? static_cast<views::View*>(custom_tab_bar_)
                         : static_cast<views::View*>(location_bar_view_);
  CHECK(bar) << "Alternate location bar impls need to handle this.";
  bar->SetVisible(visible);
}

void ToolbarView::UpdateCustomTabBarVisibility(bool visible, bool animate) {
  DCHECK_EQ(display_mode_, DisplayMode::kCustomTab);

  if (!animate) {
    size_animation_.Reset(visible ? 1.0 : 0.0);
    SetToolbarVisibility(visible);
    browser()->window()->ToolbarSizeChanged(/*is_animating=*/false);
    return;
  }

  if (visible) {
    SetToolbarVisibility(true);
    size_animation_.Show();
  } else {
    size_animation_.Hide();
  }
}

void ToolbarView::ResetTabState(WebContents* tab) {
  if (location_bar_) {
    location_bar_->ResetTabState(tab);
  }
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  AppMenuControl* app_menu_control = GetAppMenuControl();
  if (app_menu_control) {
    app_menu_control->Focus(GetAsAccessiblePaneView());
  }
}

bool ToolbarView::GetAppMenuFocused() const {
  const AppMenuControl* app_menu_control = GetAppMenuControl();
  return app_menu_control && app_menu_control->HasFocus();
}

void ToolbarView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    IntentPickerBubbleView::BubbleType bubble_type,
    const std::optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  std::optional<ui::ElementIdentifier> higlighted_element;
  if (bubble_type != IntentPickerBubbleView::BubbleType::kClickToCall) {
    if (GetIntentChipButton()) {
      higlighted_element = kIntentChipElementId;
    } else if (GetPageActionViewInterface(kActionShowIntentPicker)) {
      higlighted_element = kIntentPickerPageActionElementId;
    } else {
      return;
    }
  }

  // At this point, we either have a highlighted_element or it's a ClickToCall
  // bubble which doesn't have a corresponding page action button to highlight.
  IntentPickerBubbleView::ShowBubble(
      GetBubbleAnchor(std::nullopt), higlighted_element, bubble_type,
      GetWebContents(), std::move(app_info), show_stay_in_chrome,
      show_remember_selection, initiating_origin, std::move(callback));
}

void ToolbarView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  page_actions::PageActionViewInterface* bookmark_star_icon = nullptr;
  if (!features::IsWebUILocationBarEnabled()) {
    bookmark_star_icon = GetPageActionViewInterface(kActionBookmarkThisTab);
    CHECK(bookmark_star_icon);
  }
  BookmarkBubbleView::ShowBubble(GetBubbleAnchor(std::nullopt),
                                 GetWebContents(), bookmark_star_icon, browser_,
                                 url, already_bookmarked);
}

bool ToolbarView::IsPositionInWindowCaption(
    const gfx::Point& test_point) const {
  return IsPositionInWindowCaptionForView(this, test_point);
}

views::Button* ToolbarView::GetChromeLabsButton() const {
  return ChromeLabsCoordinator::From(browser_)->GetChromeLabsButton();
}

ExtensionsToolbarButton* ToolbarView::GetExtensionsButton() const {
  return extensions_container_->GetExtensionsButton();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

LocationBarModel* ToolbarView::GetLocationBarModel() {
  return browser_->GetFeatures().location_bar_model();
}

const LocationBarModel* ToolbarView::GetLocationBarModel() const {
  return browser_->GetFeatures().location_bar_model();
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_->GetFeatures().content_setting_bubble_model_delegate();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  DCHECK(display_mode_ == DisplayMode::kNormal);

  if ((id == IDC_BACK || id == IDC_FORWARD) &&
      features::IsWebUIBackForwardButtonEnabled()) {
    toolbar_webview_->SetBackForwardEnabled(id, enabled);
    return;
  }

  const std::array<views::Button*, 5> kButtons{back_, forward_, reload_, home_,
                                               avatar_};
  auto it = std::ranges::find_if(
      kButtons, [id](views::Button* b) { return b && b->tag() == id; });
  if (it != kButtons.end()) {
    (*it)->SetEnabled(enabled);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, ui::AcceleratorProvider implementation:

bool ToolbarView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

gfx::Size ToolbarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size;
  switch (display_mode_) {
    case DisplayMode::kCustomTab:
      size = custom_tab_bar_->GetPreferredSize();
      break;
    case DisplayMode::kLocation:
      size = location_bar_->PreferredSize();
      break;
    case DisplayMode::kNormal:
      size = AccessiblePaneView::CalculatePreferredSize(available_size);
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/41471763),
      // we cap the height at the size of known child views (location bar and
      // back button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height = std::max(location_bar_->PreferredSize().height(),
                                        GetBackForwardButtonSize().height()) +
                               layout_manager_->interior_margin().height();
        size.SetToMin({size.width(), max_height});
      }
  }
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

gfx::Size ToolbarView::GetMinimumSize() const {
  gfx::Size size;
  switch (display_mode_) {
    case DisplayMode::kCustomTab:
      size = custom_tab_bar_->GetMinimumSize();
      break;
    case DisplayMode::kLocation:
      size = location_bar_->MinimumSize();
      break;
    case DisplayMode::kNormal:
      size = AccessiblePaneView::GetMinimumSize();
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/41471763),
      // we cap the height at the size of known child views (location bar and
      // back button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height =
            std::max(location_bar_->MinimumSize().height(),
                     GetBackForwardButtonSize(/*minimum_size=*/true).height()) +
            layout_manager_->interior_margin().height();
        size.SetToMin({size.width(), max_height});
      }
      // Overflow button must be part of minimum size calculation.
      if (browser_->is_type_normal() && !overflow_button_->GetVisible()) {
        const int default_margin =
            GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
        size.Enlarge(
            default_margin + overflow_button_->GetMinimumSize().width(), 0);
      }
  }
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

void ToolbarView::Layout(PassKey) {
  // If we have not been initialized yet just do nothing.
  if (!initialized_) {
    return;
  }

  if (display_mode_ == DisplayMode::kCustomTab) {
    custom_tab_bar_->SetBounds(0, 0, width(),
                               custom_tab_bar_->GetPreferredSize().height());
    CHECK(location_bar_view_)
        << "Alternate location bar impls need to handle this.";
    location_bar_view_->SetVisible(false);
    return;
  }

  if (display_mode_ == DisplayMode::kNormal) {
    LayoutCommon();
  }

  if (toolbar_controller_) {
    // Need to determine whether the overflow button should be visible, and only
    // update it if the visibility changes.
    const bool was_overflow_button_visible =
        toolbar_controller_->overflow_button()->GetVisible();
    const bool show_overflow_button =
        toolbar_controller_->ShouldShowOverflowButton(size());
    if (was_overflow_button_visible != show_overflow_button) {
      views::ManualLayoutUtil(layout_manager_)
          .SetViewHidden(toolbar_controller_->overflow_button(),
                         !show_overflow_button);
      base::RecordAction(base::UserMetricsAction(
          show_overflow_button ? "ResponsiveToolbar.OverflowButtonShown"
                               : "ResponsiveToolbar.OverflowButtonHidden"));
    }
  }

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  LayoutSuperclass<AccessiblePaneView>(this);
}

void ToolbarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  if (!initialized_) {
    return;
  }

  if (display_mode_ == DisplayMode::kNormal) {
    LoadImages();
  }

  SchedulePaint();
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view && (focused_view->GetID() == VIEW_ID_OMNIBOX)) {
    return false;  // Let the omnibox handle all accelerator events.
  }
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

void ToolbarView::ChildPreferredSizeChanged(views::View* child) {
  InvalidateLayout();
  if (size() != GetPreferredSize()) {
    PreferredSizeChanged();
  }
}

void ToolbarView::ChildVisibilityChanged(views::View* child) {
  if (child == home_) {
    if (!home_->GetVisible() && show_home_button_.GetValue()) {
      base::UmaHistogramBoolean("Toolbar.Overflow.HomeButton", true);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar - and
// also so that it selects all content in the location bar.
views::View* ToolbarView::GetDefaultFocusableChild() {
  return location_bar_view_;
}

void ToolbarView::InitLayout() {
  const int default_margin =
      GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  const int location_bar_margin =
      GetLayoutConstant(LayoutConstant::kLocationBarMargin);

  // Shift previously flex-able elements' order by `kOrderOffset`.
  // This will cause them to be the first ones to drop out or shrink to minimum.
  // Order 1 - kOrderOffset will be assigned to new flex-able elements.
  constexpr int kOrderOffset = 1000;
  // If kOmniboxResizingPrioritization is enabled, give the location bar the
  // highest priority as it will first shrink down to its soft minimum but won't
  // hit its hard minimum until all other items have dropped out.
  const int location_bar_flex_order =
      base::FeatureList::IsEnabled(features::kOmniboxResizingPrioritization)
          ? 1
          : kOrderOffset + 1;
  constexpr int kToolbarActionsFlexOrder = kOrderOffset + 2;
  constexpr int kExtensionsFlexOrder = kOrderOffset + 3;

  const views::FlexSpecification location_bar_flex_rule =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(location_bar_flex_order);

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, default_margin));

  if (location_bar_view_) {
    location_bar_view_->SetProperty(views::kFlexBehaviorKey,
                                    location_bar_flex_rule);
    location_bar_view_->SetProperty(views::kMarginsKey,
                                    gfx::Insets::VH(0, location_bar_margin));
  } else {
    // If the location bar is part of a WebView, make that stretchable.
    toolbar_webview_->SetProperty(views::kFlexBehaviorKey,
                                  location_bar_flex_rule);
  }

  if (extensions_container_) {
    const views::FlexSpecification extensions_flex_rule =
        views::FlexSpecification(
            extensions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kExtensionsFlexOrder);

    extensions_container_->SetProperty(views::kFlexBehaviorKey,
                                       extensions_flex_rule);
  }

  if (pinned_toolbar_actions_container_) {
    pinned_toolbar_actions_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            pinned_toolbar_actions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kToolbarActionsFlexOrder));
  }

  if (toolbar_divider_) {
    toolbar_divider_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(
            0, GetLayoutConstant(LayoutConstant::kToolbarDividerSpacing)));
  }

  if (glic_button_ &&
      base::FeatureList::IsEnabled(features::kToolbarGlicButtonResizing)) {
    glic_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
            views::MaximumFlexSizeRule::kPreferred));
  }

  if (app_menu_button_ &&
      base::FeatureList::IsEnabled(features::kToolbarAppMenuLabelResizing)) {
    app_menu_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
  }

  if (avatar_ &&
      base::FeatureList::IsEnabled(features::kToolbarProfileChipResizing)) {
    // Flex order for the profile avatar button is determined by the
    // `toolbar_controller`.
    avatar_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
            views::MaximumFlexSizeRule::kPreferred));
  }

  // Order 1 is reserved for the location bar if kOmniboxResizingPrioritization
  // is enabled.
  constexpr int kToolbarFlexOrderStart = 2;

  // TODO(crbug.com/40929989): Ignore containers till issue addressed.
  toolbar_controller_ = std::make_unique<ToolbarController>(
      ToolbarController::GetDefaultResponsiveElements(browser_),
      ToolbarController::GetDefaultOverflowOrder(), kToolbarFlexOrderStart,
      this, toolbar_webview_.get(), overflow_button_, pinned_toolbar_actions_,
      PinnedToolbarActionsModel::Get(browser_view_->GetProfile()));
  overflow_button_->set_toolbar_controller(toolbar_controller_.get());

  LayoutCommon();
}

void ToolbarView::LayoutCommon() {
  DCHECK(display_mode_ == DisplayMode::kNormal);

  gfx::Insets interior_margin =
      GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN);

  auto* vts_controller = tabs::VerticalTabStripStateController::From(browser_);
  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
      (contextual_tasks::kShowEntryPoint.Get() ==
       contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) &&
      (!vts_controller || !vts_controller->ShouldDisplayVerticalTabs())) {
    interior_margin.set_left(0);
  }

  if (app_menu_button_) {
    const bool expanded = app_menu_button_->IsLabelPresentAndVisible();
    if (expanded) {
      // The interior margin in an expanded state should be more than in a
      // collapsed state.
      interior_margin.set_right(interior_margin.right() + 1);
    }
    SetRefreshMargins(app_menu_button_, expanded);
  }

  // The margins of the `avatar_` uses the same constants as the
  // `app_menu_button_`.
  if (avatar_) {
    SetRefreshMargins(avatar_, avatar_->IsLabelPresentAndVisible());
  }

  layout_manager_->SetInteriorMargin(interior_margin);

  // Extend buttons to the window edge if we're either in a maximized or
  // fullscreen window. This makes the buttons easier to hit, see Fitts' law.
  const bool extend_buttons_to_edge =
      browser_->window() && (browser_->GetWindow()->IsMaximized() ||
                             browser_->GetWindow()->IsFullscreen());
  const int margin = extend_buttons_to_edge ? interior_margin.left() : 0;
  if (features::IsWebUIBackForwardButtonEnabled()) {
    toolbar_webview_->SetBackButtonLeadingMargin(margin);
  } else {
    back_->SetLeadingMargin(margin);
  }

  const int trailing_margin =
      extend_buttons_to_edge ? interior_margin.right() : 0;
  GetAppMenuControl()->SetTrailingMargin(trailing_margin);

  if (toolbar_divider_ && extensions_container_) {
    views::ManualLayoutUtil(layout_manager_)
        .SetViewHidden(toolbar_divider_, !extensions_container_->GetVisible());
  }
  // Cast button visibility is controlled externally.
}

// AppMenuIconController::Delegate:
void ToolbarView::UpdateTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  AppMenuControl* app_menu_control = GetAppMenuControl();
  if (app_menu_control) {
    app_menu_control->SetTypeAndSeverity(type_and_severity);
  }
}

ExtensionsToolbarDesktop* ToolbarView::GetExtensionsToolbarDesktop() {
  return extensions_container_;
}

PinnedToolbarActions* ToolbarView::GetPinnedToolbarActions() {
  return pinned_toolbar_actions_;
}

gfx::Size ToolbarView::GetToolbarButtonSize() const {
  // Since DisplayMode::kLocation is for a slimline toolbar showing only compact
  // location bar used for popups, toolbar buttons (ie downloads) must be
  // smaller to accommodate the smaller size.
  const int size =
      display_mode_ == DisplayMode::kLocation
          ? location_bar_->PreferredSize().height()
          : GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  return gfx::Size(size, size);
}

views::BubbleAnchor ToolbarView::GetDefaultExtensionDialogAnchor() {
  if (extensions_container_ && extensions_container_->GetVisible()) {
    return views::BubbleAnchor(extensions_container_->GetExtensionsButton());
  }
  auto* control = GetAppMenuControl();
  return control ? control->GetAnchor() : views::BubbleAnchor();
}

PageActionIconView* ToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  if (!location_bar_view_) {
    // Only new-style page actions with `webui_location_bar_`.
    return nullptr;
  }
  return location_bar_view()->page_action_icon_controller()->GetIconView(type);
}

page_actions::PageActionViewInterface* ToolbarView::GetPageActionViewInterface(
    actions::ActionId action_id) {
  // TODO: crbug.com/501449027 -- implement for WebUI location bar.
  page_actions::PageActionPropertiesProvider provider;
  if (!provider.Contains(action_id)) {
    return nullptr;
  }
  const auto& properties = provider.GetProperties(action_id);
  if (IsPageActionMigrated(properties.type)) {
    return location_bar_view()->page_action_container()->GetPageActionView(
        action_id);
  }
  return GetPageActionIconView(properties.type);
}

AppMenuControl* ToolbarView::GetAppMenuControl() {
  if (features::IsWebUIAppMenuButtonEnabled() && toolbar_webview_) {
    return toolbar_webview_->GetAppMenuControl();
  }
  return app_menu_button_;
}

const AppMenuControl* ToolbarView::GetAppMenuControl() const {
  if (features::IsWebUIAppMenuButtonEnabled() && toolbar_webview_) {
    return toolbar_webview_->GetAppMenuControl();
  }
  return app_menu_button_;
}

gfx::Rect ToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!browser_->SupportsWindowFeature(
          Browser::WindowFeature::kFeatureLocationBar)) {
    return gfx::Rect();
  }

  CHECK(location_bar_view_)
      << "Alternate location bar impls need to handle this.";

  if (!location_bar_view_->IsDrawn()) {
    return gfx::Rect();
  }

  gfx::Rect bounds = location_bar_view_->ConvertRectToWidget(
      location_bar_view_->GetLocalBounds());
  return gfx::Rect(bounds.x(), bounds.bottom(), bounds.width(),
                   contents_bottom - bounds.bottom());
}

void ToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
  if (toolbar_webview_) {
    toolbar_webview_->AdjustForToolbarFocus();
  }
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::BubbleAnchor ToolbarView::GetBubbleAnchor(
    std::optional<actions::ActionId> action_id) {
  // If a pinned toolbar actions button exists for the action_id, return that.
  if (pinned_toolbar_actions_ && action_id.has_value() &&
      pinned_toolbar_actions_->IsActionPinnedOrPoppedOut(action_id.value())) {
    return pinned_toolbar_actions_->GetBubbleAnchor(action_id.value());
  }

  // Otherwise attempt to use the location bar.
  auto anchor = features::IsWebUILocationBarEnabled()
                    ? views::BubbleAnchor(location_bar_->GetAnchorOrNull())
                    : views::BubbleAnchor(location_bar_view_);
  bool anchor_not_drawn;
  if (views::View* view = anchor.GetIfView()) {
    anchor_not_drawn = !view->IsDrawn();
  } else {
    anchor_not_drawn = (features::IsWebUILocationBarEnabled() ||
                        features::IsWebUIPinnedToolbarActionsEnabled()) &&
                       anchor.IsNull();
  }
  // In app windows the location bar view may exist but not be drawn. Avoid
  // anchoring bubbles to a non-drawn view (e.g. on Ozone/Wayland) and always
  // return a valid view anchor by falling back to the contents view.
  if (anchor_not_drawn && browser_view_) {
    auto* top_container = browser_view_->top_container();
    CHECK(top_container);
    return views::BubbleAnchor(top_container);
  }
  return anchor;
}

views::BubbleAnchor ToolbarView::GetPageActionBubbleAnchor(
    actions::ActionId action_id) {
  page_actions::PageActionViewInterface* view =
      GetPageActionViewInterface(action_id);
  if (view) {
    return view->GetBubbleAnchor();
  }
  return views::BubbleAnchor();
}

void ToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  if (IsPageActionMigrated(PageActionIconType::kZoom)) {
    auto* zoom_view_controller = browser_->GetActiveTabInterface()
                                     ->GetTabFeatures()
                                     ->zoom_view_controller();
    CHECK(zoom_view_controller);
    zoom_view_controller->UpdatePageActionIconAndBubbleVisibility(
        /*prefer_to_show_bubble=*/can_show_bubble, /*from_user_gesture=*/false);
    return;
  }

  // Other impls are expected to only launch after page action migration.
  if (location_bar_view_) {
    location_bar_view_->page_action_icon_controller()->ZoomChangedForActiveTab(
        can_show_bubble);
  }
}

AvatarToolbarButtonInterface* ToolbarView::GetAvatarToolbarButtonInterface() {
  if (features::IsWebUIAvatarButtonEnabled()) {
    return toolbar_webview_
               ? toolbar_webview_->GetAvatarToolbarButtonInterface()
               : nullptr;
  }
  return avatar_;
}

ToolbarButton* ToolbarView::GetBackButton() {
  return back_;
}

ReloadControl* ToolbarView::GetReloadButton() {
  if (features::IsWebUIReloadButtonEnabled()) {
    if (toolbar_webview_) {
      return toolbar_webview_->GetReloadControl();
    } else {
      return nullptr;
    }
  }
  return reload_;
}

IntentChipButton* ToolbarView::GetIntentChipButton() {
  return location_bar_view() ? location_bar_view()->intent_chip() : nullptr;
}

ToolbarButton* ToolbarView::GetDownloadButton() {
  return pinned_toolbar_actions_container_
             ? pinned_toolbar_actions_container_->GetButtonFor(
                   kActionShowDownloads)
             : nullptr;
}

WebUIToolbarWebView* ToolbarView::GetWebUIToolbarViewForTesting() {
  return toolbar_webview_;
}

std::optional<BrowserRootView::DropIndex> ToolbarView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return BrowserRootView::DropIndex{
      .index = browser_->tab_strip_model()->active_index(),
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex};
}

BrowserRootView::DropTarget* ToolbarView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  return HitTestPoint(loc_in_local_coords) ? this : nullptr;
}

views::View* ToolbarView::GetViewForDrop() {
  return this;
}

void ToolbarView::OnChromeLabsPrefChanged() {
  actions::ActionItem* chrome_labs_action =
      pinned_toolbar_actions_container_->GetActionItemFor(
          kActionShowChromeLabs);
  chrome_labs_action->SetVisible(show_chrome_labs_button_.GetValue() &&
                                 ShouldShowChromeLabsUI(browser_->profile()));
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      chrome_labs_action->GetVisible()
          ? IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_ADDED_BY_ENTERPRISE_POLICY
          : IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_REMOVED_BY_ENTERPRISE_POLICY));
}

void ToolbarView::LoadImages() {
  DCHECK_EQ(display_mode_, DisplayMode::kNormal);

  if (extensions_container_) {
    extensions_container_->UpdateAllIcons();
  }
}

void ToolbarView::OnShowForwardButtonChanged() {
  SetForwardButtonVisibility(show_forward_button_.GetValue());
  InvalidateLayout();
}

void ToolbarView::OnShowHomeButtonChanged() {
  if (home_) {
    home_->SetVisible(show_home_button_.GetValue());
  }
}

void ToolbarView::OnTouchUiChanged() {
  if (display_mode_ == DisplayMode::kNormal) {
    // Update the internal margins for touch layout.
    // TODO(dfried): I think we can do better than this by making the touch UI
    // code cleaner.
    const int default_margin =
        GetLayoutConstant(LayoutConstant::kToolbarElementPadding);
    const int location_bar_margin =
        GetLayoutConstant(LayoutConstant::kLocationBarMargin);
    layout_manager_->SetDefault(views::kMarginsKey,
                                gfx::Insets::VH(0, default_margin));
    if (location_bar_view_) {
      location_bar_view_->SetProperty(views::kMarginsKey,
                                      gfx::Insets::VH(0, location_bar_margin));
    }

    LoadImages();
    PreferredSizeChanged();
  }
}

void ToolbarView::SetForwardButtonVisibility(bool visible) {
  if (features::IsWebUIBackForwardButtonEnabled()) {
    toolbar_webview_->SetForwardVisible(visible);
  } else {
    forward_->SetVisible(visible);
  }
}

gfx::Size ToolbarView::GetBackForwardButtonSize(bool minimum_size) const {
  if (back_) {
    return minimum_size ? back_->GetMinimumSize() : back_->GetPreferredSize();
  }
  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  return gfx::Size(size, size);
}

BEGIN_METADATA(ToolbarView)
ADD_READONLY_PROPERTY_METADATA(bool, AppMenuFocused)
END_METADATA
