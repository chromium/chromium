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
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
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
#include "chrome/browser/ui/tab_search_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
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
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
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
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
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
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if defined(USE_AURA)
#include "ui/aura/window_occlusion_tracker.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActionItemUnderlineIndicatorKey, false)

namespace {

// Intermediate data for determining whether a point should be considered in the
// caption are when the toolbar is the top element in the browser.
struct CaptionHitTestData {
  raw_ptr<const views::View> at = nullptr;
  bool is_direct_hit = false;
  raw_ptr<const views::View> before = nullptr;
  int before_dist = 0;
  raw_ptr<const views::View> after = nullptr;
  int after_dist = 0;
};

// Calculates the `CaptionHitTestData` (`data`) starting at `view` for `point`,
// which should be in `view`'s local coordinates. Bails out immediately if a
// View is hit; may traverse into icon containers.
void CalculateIsPositionInWindowCaption(CaptionHitTestData& data,
                                        const views::View* view,
                                        const gfx::Point& point) {
  for (auto& child : view->children()) {
    if (!child->GetVisible()) {
      continue;
    }
    const gfx::Rect bounds = child->bounds();

    if (views::IsViewClass<ToolbarIconContainerView>(child) ||
        views::IsViewClass<page_actions::PageActionContainerView>(child)) {
      // Traverse into known icon containers.
      const auto in_child =
          views::View::ConvertPointToTarget(view, child, point);
      CalculateIsPositionInWindowCaption(data, child, in_child);
    } else if (bounds.x() <= point.x() && bounds.right() >= point.x()) {
      // This point is in/above/below the child.
      data.at = child;
      data.is_direct_hit = bounds.Contains(point);
    } else {
      // See if the view is the closest before or after the target point in the
      // layout.
      if (bounds.right() < point.x()) {
        const int dist = point.x() - bounds.right();
        if (!data.before || data.before_dist > dist) {
          data.before = child;
          data.before_dist = dist;
        }
      } else if (bounds.x() > point.x()) {
        const int dist = bounds.x() - point.x();
        if (!data.after || data.after_dist > dist) {
          data.after = child;
          data.after_dist = dist;
        }
      }
    }

    // If a view was hit at any level, stop processing.
    if (data.at) {
      break;
    }
  }
}

// Returns whether `point` should be treated as part of the caption area in
// `view`, which should be the topmost view in the browser.
bool IsPositionInWindowCaption(const views::View* view,
                               const gfx::Point& point) {
  CaptionHitTestData data;
  CalculateIsPositionInWindowCaption(data, view, point);

  const bool is_above_centerline =
      point.y() <= view->GetLocalBounds().CenterPoint().y();
  const auto is_separator = [](const views::View* view) {
    return views::IsViewClass<views::Separator>(view) ||
           views::IsViewClass<ToolbarDivider>(view);
  };

  // If the point is in a view, then it's not in the caption unless the view is
  // a separator. If the point is at a view but not in it, then it is caption if
  // the point is centerline; otherwise it's not.
  if (data.at) {
    return is_separator(data.at) ||
           (!data.is_direct_hit && is_above_centerline);
  }

  // If the point is not in a view but it is next to a separator or the edge of
  // the toolbar, it is caption.
  if (!data.before || is_separator(data.before) || !data.after ||
      is_separator(data.after)) {
    return true;
  }

  // All remaining points (between non-separator views) are caption if they are
  // above centerline.
  return is_above_centerline;
}

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

bool IsMigratedClickToCallBubble(
    IntentPickerBubbleView::BubbleType bubble_type) {
  return bubble_type == IntentPickerBubbleView::BubbleType::kClickToCall &&
         IsPageActionMigrated(PageActionIconType::kClickToCall);
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
}

ToolbarView::~ToolbarView() {
  if (display_mode_ != DisplayMode::kNormal) {
    return;
  }

  overflow_button_->set_toolbar_controller(nullptr);

  for (const auto& view_and_command : GetViewCommandMap()) {
    chrome::RemoveCommandObserver(browser_, view_and_command.second, this);
  }
}

void ToolbarView::Init() {
#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/1183894#c2
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif

  auto location_bar = std::make_unique<LocationBarView>(
      browser_, browser_->profile(), browser_->command_controller(), this,
      display_mode_ != DisplayMode::kNormal);
  // Make sure the toolbar shows by default.
  size_animation_.Reset(1);

  if (display_mode_ != DisplayMode::kNormal) {
    location_bar_view_ = AddChildView(std::move(location_bar));
    location_bar_view_->Init();
    location_bar_ = location_bar_view_;
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
    SetBackground(views::CreateSolidBackground(kColorLocationBarBackground));
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero))
        .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);
    CHECK(location_bar_view_)
        << "Alternate location bar impls need to handle this.";
    location_bar_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
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
  if (!browser_->profile()->IsGuestSession()) {
    extensions_container = std::make_unique<ExtensionsToolbarDesktop>(browser_);

    toolbar_divider = std::make_unique<ToolbarDivider>();
  }

  std::unique_ptr<MediaToolbarButtonView> media_button;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControls)) {
    media_button = std::make_unique<MediaToolbarButtonView>(
        browser_view_,
        std::make_unique<MediaToolbarButtonContextualMenu>(browser_));
  }

  // Always add children in order from left to right, for accessibility.

  back_ = AddChildView(std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kBack,
      base::BindRepeating(callback, browser_, IDC_BACK), browser_));

  forward_ = AddChildView(std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kForward,
      base::BindRepeating(callback, browser_, IDC_FORWARD), browser_));

  if (features::IsWebUIToolbarEnabled()) {
    toolbar_webview_ = AddChildView(std::make_unique<WebUIToolbarWebView>(
        browser_, browser_->command_controller()));
  }

  if (!features::IsWebUIReloadButtonEnabled()) {
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
      ((contextual_tasks::kShowEntryPoint.Get() ==
        contextual_tasks::EntryPointOption::kToolbarPermanent) ||
       (contextual_tasks::kShowEntryPoint.Get() ==
        contextual_tasks::EntryPointOption::kToolbarRevisit))) {
    AddChildView(std::make_unique<ContextualTasksButton>(browser_));
  }

  location_bar_view_ = AddChildView(std::move(location_bar));
  location_bar_ = location_bar_view_;

  if (extensions_container) {
    extensions_container_ = AddChildView(std::move(extensions_container));
    extensions_toolbar_coordinator_ =
        std::make_unique<ExtensionsToolbarCoordinator>(browser_,
                                                       extensions_container_);
  }

  if (toolbar_divider) {
    toolbar_divider_ = AddChildView(std::move(toolbar_divider));
  }

  pinned_toolbar_actions_container_ = AddChildView(
      std::make_unique<PinnedToolbarActionsContainer>(browser_view_, this));

  if (!base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton) &&
      features::HasTabSearchToolbarButton()) {
    tab_search_button_ =
        pinned_toolbar_actions_container()->CreatePermanentButtonFor(
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
  if (!performance_manager::user_tuning::IsBatterySaverModeManagedByOS()) {
    battery_saver_button_ =
        AddChildView(std::make_unique<BatterySaverButton>(browser_view_));
  }

  performance_intervention_button_ = AddChildView(
      std::make_unique<PerformanceInterventionButton>(browser_view_));

  if (media_button) {
    media_button_ = AddChildView(std::move(media_button));
  }

  avatar_ = AddChildView(std::make_unique<AvatarToolbarButton>(browser_view_));
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

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  auto new_tab_button = std::make_unique<ToolbarButton>(base::BindRepeating(
      &ToolbarView::NewTabButtonPressed, base::Unretained(this)));
  new_tab_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  new_tab_button->SetVectorIcon(kNewTabToolbarButtonIcon);
  new_tab_button->SetVisible(false);
  new_tab_button->SetProperty(views::kElementIdentifierKey,
                              kToolbarNewTabButtonElementId);
  new_tab_button_ = AddChildView(std::move(new_tab_button));
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  overflow_button_ = AddChildView(std::make_unique<OverflowButton>());
  overflow_button_->SetVisible(false);

  auto app_menu_button = std::make_unique<BrowserAppMenuButton>(this);
  app_menu_button->SetFlipCanvasOnPaintForRTLUI(true);
  app_menu_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
  app_menu_button->SetID(VIEW_ID_APP_MENU);
  app_menu_button_ = AddChildView(std::move(app_menu_button));

  LoadImages();

  // Set the button icon based on the system state. Do this after
  // |app_menu_button_| has been added as a bubble may be shown that needs
  // the widget (widget found by way of app_menu_button_->GetWidget()).
  app_menu_icon_controller_.UpdateDelegate();

  if (location_bar_view_) {
    location_bar_view_->Init();
  }

  show_forward_button_.Init(
      prefs::kShowForwardButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowForwardButtonChanged,
                          base::Unretained(this)));

  forward_->SetVisible(show_forward_button_.GetValue());

  show_home_button_.Init(
      prefs::kShowHomeButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowHomeButtonChanged,
                          base::Unretained(this)));

  if (home_) {
    home_->SetVisible(show_home_button_.GetValue());
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

void ToolbarView::UpdateForWebUITabStrip() {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (!new_tab_button_) {
    return;
  }
  if (browser_view_->webui_tab_strip()) {
    const int button_height =
        GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
    new_tab_button_->SetPreferredSize(gfx::Size(button_height, button_height));
    new_tab_button_->SetVisible(true);
    const size_t insertion_index = GetIndexOf(new_tab_button_).value();
    AddChildViewAt(browser_view_->webui_tab_strip()->CreateTabCounter(),
                   insertion_index);
    LoadImages();
  } else {
    new_tab_button_->SetVisible(false);
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

void ToolbarView::ResetTabState(WebContents* tab) {
  if (location_bar_) {
    location_bar_->ResetTabState(tab);
  }
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  if (app_menu_button_) {
    SetPaneFocus(app_menu_button_);
  }
}

bool ToolbarView::GetAppMenuFocused() const {
  return app_menu_button_ && app_menu_button_->HasFocus();
}

void ToolbarView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    IntentPickerBubbleView::BubbleType bubble_type,
    const std::optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  views::Button* highlighted_button = nullptr;
  if (bubble_type == IntentPickerBubbleView::BubbleType::kClickToCall) {
    highlighted_button =
        GetPageActionIconView(PageActionIconType::kClickToCall);
  } else if (highlighted_button = GetIntentChipButton(); !highlighted_button) {
    highlighted_button = GetPageActionView(kActionShowIntentPicker);
  }

  // Post migration, highlighted_button is a nullptr for ClickToCall
  // BubbleType but the bubble still gets shown without a page action being
  // shown/highlighted.
  if (highlighted_button || IsMigratedClickToCallBubble(bubble_type)) {
    IntentPickerBubbleView::ShowBubble(
        location_bar_view(), highlighted_button, bubble_type, GetWebContents(),
        std::move(app_info), show_stay_in_chrome, show_remember_selection,
        initiating_origin, std::move(callback));
  }
}

void ToolbarView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  views::View* const anchor_view = location_bar_view();
  views::Button* const bookmark_star_icon =
      GetPageActionView(kActionBookmarkThisTab);
  CHECK(bookmark_star_icon);
  BookmarkBubbleView::ShowBubble(anchor_view, GetWebContents(),
                                 bookmark_star_icon, browser_, url,
                                 already_bookmarked);
}

bool ToolbarView::IsPositionInWindowCaption(
    const gfx::Point& test_point) const {
  return ::IsPositionInWindowCaption(this, test_point);
}

views::Button* ToolbarView::GetChromeLabsButton() const {
  return browser_->GetFeatures()
      .chrome_labs_coordinator()
      ->GetChromeLabsButton();
}

ExtensionsToolbarButton* ToolbarView::GetExtensionsButton() const {
  return extensions_container_->GetExtensionsButton();
}

ToolbarButton* ToolbarView::GetCastButton() const {
  return pinned_toolbar_actions_container()
             ? pinned_toolbar_actions_container()->GetButtonFor(
                   kActionRouteMedia)
             : nullptr;
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
      // the toolbar to report an unreasonable height (see crbug.com/985909), we
      // cap the height at the size of known child views (location bar and back
      // button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height = std::max(location_bar_->PreferredSize().height(),
                                        back_->GetPreferredSize().height()) +
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
      // the toolbar to report an unreasonable height (see crbug.com/985909), we
      // cap the height at the size of known child views (location bar and back
      // button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height = std::max(location_bar_->MinimumSize().height(),
                                        back_->GetMinimumSize().height()) +
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

void ToolbarView::NewTabButtonPressed(const ui::Event& event) {
  chrome::NewTab(browser_view_->browser(),
                 NewTabTypes::kNewTabButtonInToolbarForTouch);
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
  // TODO(dfried): rename this constant.
  const int location_bar_margin =
      GetLayoutConstant(LayoutConstant::kToolbarStandardSpacing);

  // Shift previously flex-able elements' order by `kOrderOffset`.
  // This will cause them to be the first ones to drop out or shrink to minimum.
  // Order 1 - kOrderOffset will be assigned to new flex-able elements.
  constexpr int kOrderOffset = 1000;
  constexpr int kLocationBarFlexOrder = kOrderOffset + 1;
  constexpr int kToolbarActionsFlexOrder = kOrderOffset + 2;
  constexpr int kExtensionsFlexOrder = kOrderOffset + 3;

  const views::FlexSpecification location_bar_flex_rule =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(kLocationBarFlexOrder);

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

  constexpr int kToolbarFlexOrderStart = 1;

  // TODO(crbug.com/40929989): Ignore containers till issue addressed.
  toolbar_controller_ = std::make_unique<ToolbarController>(
      ToolbarController::GetDefaultResponsiveElements(browser_),
      ToolbarController::GetDefaultOverflowOrder(), kToolbarFlexOrderStart,
      this, overflow_button_, pinned_toolbar_actions_container_,
      PinnedToolbarActionsModel::Get(browser_view_->GetProfile()));
  overflow_button_->set_toolbar_controller(toolbar_controller_.get());

  LayoutCommon();
}

void ToolbarView::LayoutCommon() {
  DCHECK(display_mode_ == DisplayMode::kNormal);

  gfx::Insets interior_margin =
      GetLayoutInsets(browser_view_->webui_tab_strip()
                          ? LayoutInset::WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN
                          : LayoutInset::TOOLBAR_INTERIOR_MARGIN);

  if (!browser_view_->webui_tab_strip()) {
    if (app_menu_button_->IsLabelPresentAndVisible()) {
      // The interior margin in an expanded state should be more than in a
      // collapsed state.
      interior_margin.set_right(interior_margin.right() + 1);
      app_menu_button_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::VH(0, kBrowserAppMenuRefreshExpandedMargin));
    } else {
      app_menu_button_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::VH(0, kBrowserAppMenuRefreshCollapsedMargin));
    }

    // The margins of the `avatar_` uses the same constants as the
    // `app_menu_button_`.
    if (avatar_->IsLabelPresentAndVisible()) {
      avatar_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::VH(0, kBrowserAppMenuRefreshExpandedMargin));
    } else {
      avatar_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::VH(0, kBrowserAppMenuRefreshCollapsedMargin));
    }
  }

  layout_manager_->SetInteriorMargin(interior_margin);

  // Extend buttons to the window edge if we're either in a maximized or
  // fullscreen window. This makes the buttons easier to hit, see Fitts' law.
  const bool extend_buttons_to_edge =
      browser_->window() &&
      (browser_->window()->IsMaximized() || browser_->window()->IsFullscreen());
  back_->SetLeadingMargin(extend_buttons_to_edge ? interior_margin.left() : 0);
  app_menu_button_->SetTrailingMargin(
      extend_buttons_to_edge ? interior_margin.right() : 0);

  if (toolbar_divider_ && extensions_container_) {
    views::ManualLayoutUtil(layout_manager_)
        .SetViewHidden(toolbar_divider_, !extensions_container_->GetVisible());
  }
  // Cast button visibility is controlled externally.
}

// AppMenuIconController::Delegate:
void ToolbarView::UpdateTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  // There's no app menu in tabless windows.
  if (!app_menu_button_) {
    return;
  }

  std::u16string accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (type_and_severity.type ==
      AppMenuIconController::IconType::kUpgradeNotification) {
    accname_app = l10n_util::GetStringFUTF16(
        IDS_ACCNAME_APP_UPGRADE_RECOMMENDED, accname_app);
  }
  app_menu_button_->GetViewAccessibility().SetName(accname_app);
  app_menu_button_->SetTypeAndSeverity(type_and_severity);
}

ExtensionsToolbarDesktop* ToolbarView::GetExtensionsToolbarDesktop() {
  return extensions_container_;
}

PinnedToolbarActionsContainer* ToolbarView::GetPinnedToolbarActionsContainer() {
  return pinned_toolbar_actions_container_;
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

views::View* ToolbarView::GetDefaultExtensionDialogAnchorView() {
  if (extensions_container_ && extensions_container_->GetVisible()) {
    return extensions_container_->GetExtensionsButton();
  }
  return GetAppMenuButton();
}

PageActionIconView* ToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return location_bar_view()->page_action_icon_controller()->GetIconView(type);
}

IconLabelBubbleView* ToolbarView::GetPageActionView(
    actions::ActionId action_id) {
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

AppMenuButton* ToolbarView::GetAppMenuButton() {
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
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* ToolbarView::GetAnchorView(
    std::optional<actions::ActionId> action_id) {
  if (pinned_toolbar_actions_container_ && action_id.has_value() &&
      pinned_toolbar_actions_container_->IsActionPinnedOrPoppedOut(
          action_id.value())) {
    return pinned_toolbar_actions_container_->GetButtonFor(action_id.value());
  }

  return location_bar_view_;
}

views::BubbleAnchor ToolbarView::GetBubbleAnchor(
    std::optional<actions::ActionId> action_id) {
  if (views::View* view = GetAnchorView(action_id)) {
    return view;
  }
  return nullptr;
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

  CHECK(location_bar_view_)
      << "Alternate location bar impls need to handle this.";
  location_bar_view_->page_action_icon_controller()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* ToolbarView::GetAvatarToolbarButton() {
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
  return location_bar_view()->intent_chip();
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
  forward_->SetVisible(show_forward_button_.GetValue());
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
        GetLayoutConstant(LayoutConstant::kToolbarStandardSpacing);
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

BEGIN_METADATA(ToolbarView)
ADD_READONLY_PROPERTY_METADATA(bool, AppMenuFocused)
END_METADATA
