// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_icon_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_node_data.h"
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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/recovery/recovery_install_global_error_factory.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if defined(USE_AURA)
#include "ui/aura/window_occlusion_tracker.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

// Gets the display mode for a given browser.
ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser->is_type_custom_tab())
    return ToolbarView::DisplayMode::CUSTOM_TAB;
#endif

  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB
  // display mode.
  if (web_app::AppBrowserController::IsWebApp(browser))
    return ToolbarView::DisplayMode::CUSTOM_TAB;

  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return ToolbarView::DisplayMode::NORMAL;

  return ToolbarView::DisplayMode::LOCATION;
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

constexpr int kToolbarDividerWidth = 2;
constexpr int kToolbarDividerHeight = 16;
constexpr int kToolbarDividerCornerRadius = 1;
constexpr int kToolbarDividerSpacing = 9;
constexpr int kBrowserAppMenuRefreshExpandedMargin = 5;
constexpr int kBrowserAppMenuRefreshCollapsedMargin = 2;

// Draws background akin to the tabstrip.
class TabstripLikeBackground : public views::Background {
 public:
  explicit TabstripLikeBackground(BrowserView* browser_view)
      : browser_view_(browser_view) {}

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    bool painted = TopContainerBackground::PaintThemeCustomImage(
        canvas, view, browser_view_, /*translate_view_coordinates=*/false);
    if (!painted) {
      SkColor frame_color =
          browser_view_->frame()->GetFrameView()->GetFrameColor(
              BrowserFrameActiveState::kUseCurrent);
      canvas->DrawColor(frame_color);
    }
  }

  const raw_ptr<BrowserView> browser_view_;
};

}  // namespace

class ToolbarView::ContainerView : public views::View {
 public:
  // Calling PreferredSizeChanged() will trigger the parent's
  // ChildPreferredSizeChanged.
  // Bubble up calls to ChildPreferredSizeChanged.
  void ChildPreferredSizeChanged(View* child) override {
    PreferredSizeChanged();
  }
};

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser, BrowserView* browser_view)
    : AnimationDelegateViews(this),
      browser_(browser),
      browser_view_(browser_view),
      app_menu_icon_controller_(browser->profile(), this),
      display_mode_(GetDisplayMode(browser)) {
  SetID(VIEW_ID_TOOLBAR);

  container_view_ = AddChildView(std::make_unique<ContainerView>());

  if (display_mode_ == DisplayMode::NORMAL) {
    container_view_->SetBackground(
        std::make_unique<TopContainerBackground>(browser_view));

    for (const auto& view_and_command : GetViewCommandMap())
      chrome::AddCommandObserver(browser_, view_and_command.second, this);
  }
  views::SetCascadingColorProviderColor(
      container_view_, views::kCascadingBackgroundColor, kColorToolbar);
}

ToolbarView::~ToolbarView() {
  if (display_mode_ != DisplayMode::NORMAL)
    return;

  for (const auto& view_and_command : GetViewCommandMap())
    chrome::RemoveCommandObserver(browser_, view_and_command.second, this);
}

void ToolbarView::Init() {
#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/1183894#c2
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif

  // The background views must be behind container_view_.
  if (features::IsChromeRefresh2023()) {
    background_view_left_ = AddChildViewAt(std::make_unique<View>(), 0);
    background_view_left_->SetBackground(
        std::make_unique<TabstripLikeBackground>(browser_view_));
    background_view_right_ = AddChildViewAt(std::make_unique<View>(), 0);
    background_view_right_->SetBackground(
        std::make_unique<TabstripLikeBackground>(browser_view_));

    active_state_subscription_ =
        GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &ToolbarView::ActiveStateChanged, base::Unretained(this)));
  }

  auto location_bar = std::make_unique<LocationBarView>(
      browser_, browser_->profile(), browser_->command_controller(), this,
      display_mode_ != DisplayMode::NORMAL);
  // Make sure the toolbar shows by default.
  size_animation_.Reset(1);

  std::unique_ptr<DownloadToolbarButtonView> download_button;
  if (download::IsDownloadBubbleEnabled(browser_->profile())) {
    download_button =
        std::make_unique<DownloadToolbarButtonView>(browser_view_);
  }

  if (display_mode_ != DisplayMode::NORMAL) {
    location_bar_ = container_view_->AddChildView(std::move(location_bar));
    location_bar_->Init();
  }

  if (display_mode_ == DisplayMode::CUSTOM_TAB) {
    custom_tab_bar_ = container_view_->AddChildView(
        std::make_unique<CustomTabBarView>(browser_view_, this));
    container_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    initialized_ = true;
    return;
  } else if (display_mode_ == DisplayMode::LOCATION) {
    // Add the download button for popups.
    if (download_button) {
      download_button_ =
          container_view_->AddChildView(std::move(download_button));
      download_button_->SetPreferredSize(
          gfx::Size(location_bar_->GetPreferredSize().height(),
                    location_bar_->GetPreferredSize().height()));
      download_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
      // Hide the icon by default; it will show up when there's a download.
      download_button_->Hide();
    }
    container_view_->SetBackground(
        views::CreateThemedSolidBackground(kColorLocationBarBackground));
    container_view_->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero))
        .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);
    location_bar_->SetProperty(
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

  std::unique_ptr<ToolbarButton> back = std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kBack,
      base::BindRepeating(callback, browser_, IDC_BACK), browser_);

  std::unique_ptr<ToolbarButton> forward = std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kForward,
      base::BindRepeating(callback, browser_, IDC_FORWARD), browser_);

  std::unique_ptr<ReloadButton> reload =
      std::make_unique<ReloadButton>(browser_->command_controller());

  PrefService* const prefs = browser_->profile()->GetPrefs();
  std::unique_ptr<HomeButton> home = std::make_unique<HomeButton>(
      base::BindRepeating(callback, browser_, IDC_HOME), prefs);

  std::unique_ptr<ExtensionsToolbarContainer> extensions_container;
  std::unique_ptr<views::View> toolbar_divider;

  // Do not create the extensions or browser actions container if it is a guest
  // profile (only regular and incognito profiles host extensions).
  if (!browser_->profile()->IsGuestSession()) {
    extensions_container =
        std::make_unique<ExtensionsToolbarContainer>(browser_);

    if (features::IsChromeRefresh2023()) {
      toolbar_divider = std::make_unique<views::View>();
    }
  }
  std::unique_ptr<media_router::CastToolbarButton> cast;
  if (media_router::MediaRouterEnabled(browser_->profile()))
    cast = media_router::CastToolbarButton::Create(browser_);

  std::unique_ptr<MediaToolbarButtonView> media_button;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControls)) {
    media_button = std::make_unique<MediaToolbarButtonView>(
        browser_view_, MediaToolbarButtonContextualMenu::Create(browser_));
  }

  std::unique_ptr<send_tab_to_self::SendTabToSelfToolbarIconView>
      send_tab_to_self_button;
  if (!browser_->profile()->IsOffTheRecord()) {
    send_tab_to_self_button =
        std::make_unique<send_tab_to_self::SendTabToSelfToolbarIconView>(
            browser_view_);
  }

  std::unique_ptr<SidePanelToolbarButton> side_panel_button;
  std::unique_ptr<SidePanelToolbarContainer> side_panel_toolbar_container;
  if (browser_view_->unified_side_panel()) {
    if (companion::IsCompanionFeatureEnabled()) {
      side_panel_toolbar_container =
          std::make_unique<SidePanelToolbarContainer>(browser_view_);
    } else {
      side_panel_button = std::make_unique<SidePanelToolbarButton>(browser_);
    }
  }

  // Always add children in order from left to right, for accessibility.
  back_ = container_view_->AddChildView(std::move(back));
  forward_ = container_view_->AddChildView(std::move(forward));
  reload_ = container_view_->AddChildView(std::move(reload));
  home_ = container_view_->AddChildView(std::move(home));

  location_bar_ = container_view_->AddChildView(std::move(location_bar));

  if (extensions_container)
    extensions_container_ =
        container_view_->AddChildView(std::move(extensions_container));

  if (toolbar_divider) {
    toolbar_divider_ =
        container_view_->AddChildView(std::move(toolbar_divider));
    toolbar_divider_->SetPreferredSize(
        gfx::Size(kToolbarDividerWidth, kToolbarDividerHeight));
  }

  if (base::FeatureList::IsEnabled(features::kChromeLabs)) {
    chrome_labs_model_ = std::make_unique<ChromeLabsModel>();
    UpdateChromeLabsNewBadgePrefs(browser_->profile(),
                                  chrome_labs_model_.get());
    if (ShouldShowChromeLabsUI(chrome_labs_model_.get(), browser_->profile())) {
      chrome_labs_button_ =
          container_view_->AddChildView(std::make_unique<ChromeLabsButton>(
              browser_view_, chrome_labs_model_.get()));

      show_chrome_labs_button_.Init(
          chrome_labs_prefs::kBrowserLabsEnabled, prefs,
          base::BindRepeating(&ToolbarView::OnChromeLabsPrefChanged,
                              base::Unretained(this)));
      // Set the visibility for the button based on initial enterprise policy
      // value. Only call OnChromeLabsPrefChanged if there is a change from the
      // initial value.
      chrome_labs_button_->SetVisible(show_chrome_labs_button_.GetValue());
    }
  }

  battery_saver_button_ = container_view_->AddChildView(
      std::make_unique<BatterySaverButton>(browser_view_));

  if (cast)
    cast_ = container_view_->AddChildView(std::move(cast));

  if (media_button)
    media_button_ = container_view_->AddChildView(std::move(media_button));

  if (download_button)
    download_button_ =
        container_view_->AddChildView(std::move(download_button));

  if (send_tab_to_self_button)
    send_tab_to_self_button_ =
        container_view_->AddChildView(std::move(send_tab_to_self_button));

  if (side_panel_toolbar_container) {
    side_panel_container_ =
        container_view_->AddChildView(std::move(side_panel_toolbar_container));
  } else if (side_panel_button) {
    side_panel_button_ =
        container_view_->AddChildView(std::move(side_panel_button));
  }

  avatar_ = container_view_->AddChildView(
      std::make_unique<AvatarToolbarButton>(browser_view_));
  bool show_avatar_toolbar_button = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS only badges Incognito, Guest, and captive portal signin icons in
  // the browser window.
  show_avatar_toolbar_button =
      browser_->profile()->IsIncognitoProfile() ||
      browser_->profile()->IsGuestSession() ||
      (browser_->profile()->IsOffTheRecord() &&
       browser_->profile()->GetOTRProfileID().IsCaptivePortal());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  show_avatar_toolbar_button = !profiles::IsPublicSession();
#endif
  avatar_->SetVisible(show_avatar_toolbar_button);

  auto app_menu_button = std::make_unique<BrowserAppMenuButton>(this);
  app_menu_button->SetFlipCanvasOnPaintForRTLUI(true);
  app_menu_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
  app_menu_button->SetID(VIEW_ID_APP_MENU);
  app_menu_button_ = container_view_->AddChildView(std::move(app_menu_button));

  LoadImages();

  // Start global error services now so we set the icon on the menu correctly.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  RecoveryInstallGlobalErrorFactory::GetForProfile(browser_->profile());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // Set the button icon based on the system state. Do this after
  // |app_menu_button_| has been added as a bubble may be shown that needs
  // the widget (widget found by way of app_menu_button_->GetWidget()).
  app_menu_icon_controller_.UpdateDelegate();

  location_bar_->Init();

  show_home_button_.Init(
      prefs::kShowHomeButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowHomeButtonChanged,
                          base::Unretained(this)));

  home_->SetVisible(show_home_button_.GetValue());

  InitLayout();

  for (auto* button : std::array<views::Button*, 5>{back_, forward_, reload_,
                                                    home_, avatar_}) {
    if (button)
      button->set_tag(GetViewCommandMap().at(button->GetID()));
  }

  initialized_ = true;
}

void ToolbarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0)
    SetToolbarVisibility(false);
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/false);
}

void ToolbarView::AnimationProgressed(const gfx::Animation* animation) {
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/true);
}

void ToolbarView::Update(WebContents* tab) {
  if (location_bar_)
    location_bar_->Update(tab);

  if (extensions_container_)
    extensions_container_->UpdateAllIcons();

  if (side_panel_container_) {
    side_panel_container_->UpdateAllIcons();
  }

  if (reload_)
    reload_->SetMenuEnabled(chrome::IsDebuggerAttachedToCurrentTab(browser_));
}

void ToolbarView::SetToolbarVisibility(bool visible) {
  SetVisible(visible);
  views::View* bar = display_mode_ == DisplayMode::CUSTOM_TAB
                         ? static_cast<views::View*>(custom_tab_bar_)
                         : static_cast<views::View*>(location_bar_);

  bar->SetVisible(visible);
}

void ToolbarView::UpdateCustomTabBarVisibility(bool visible, bool animate) {
  DCHECK_EQ(display_mode_, DisplayMode::CUSTOM_TAB);

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
  if (browser_view_->webui_tab_strip() && app_menu_button_) {
    const size_t insertion_index =
        container_view_->GetIndexOf(app_menu_button_).value();
    container_view_->AddChildViewAt(
        browser_view_->webui_tab_strip()->CreateNewTabButton(),
        insertion_index);
    container_view_->AddChildViewAt(
        browser_view_->webui_tab_strip()->CreateTabCounter(), insertion_index);
    LoadImages();
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

void ToolbarView::ResetTabState(WebContents* tab) {
  if (location_bar_)
    location_bar_->ResetTabState(tab);
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  if (app_menu_button_)
    SetPaneFocus(app_menu_button_);
}

bool ToolbarView::GetAppMenuFocused() const {
  return app_menu_button_ && app_menu_button_->HasFocus();
}

void ToolbarView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    IntentPickerBubbleView::BubbleType bubble_type,
    const absl::optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  views::Button* highlighted_button = nullptr;
  if (bubble_type == IntentPickerBubbleView::BubbleType::kClickToCall) {
    highlighted_button =

        GetPageActionIconView(PageActionIconType::kClickToCall);
  } else if (apps::features::LinkCapturingUiUpdateEnabled()) {
    highlighted_button = GetIntentChipButton();
  } else {
    highlighted_button =
        GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  if (!highlighted_button)
    return;

  IntentPickerBubbleView::ShowBubble(
      location_bar(), highlighted_button, bubble_type, GetWebContents(),
      std::move(app_info), show_stay_in_chrome, show_remember_selection,
      initiating_origin, std::move(callback));
}

void ToolbarView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  views::View* const anchor_view = location_bar();
  PageActionIconView* const bookmark_star_icon =
      GetPageActionIconView(PageActionIconType::kBookmarkStar);

  std::unique_ptr<BubbleSyncPromoDelegate> delegate;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  delegate =
      std::make_unique<BookmarkBubbleSignInDelegate>(browser_->profile());
#endif
  BookmarkBubbleView::ShowBubble(anchor_view, GetWebContents(),
                                 bookmark_star_icon, std::move(delegate),
                                 browser_, url, already_bookmarked);
}

ExtensionsToolbarButton* ToolbarView::GetExtensionsButton() const {
  return extensions_container_->GetExtensionsButton();
}

SidePanelToolbarButton* ToolbarView::GetSidePanelButton() {
  if (side_panel_container_) {
    return side_panel_container_->GetSidePanelButton();
  }
  return side_panel_button_;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

LocationBarModel* ToolbarView::GetLocationBarModel() {
  return browser_->location_bar_model();
}

const LocationBarModel* ToolbarView::GetLocationBarModel() const {
  return browser_->location_bar_model();
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_->content_setting_bubble_model_delegate();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  DCHECK(display_mode_ == DisplayMode::NORMAL);
  const std::array<views::Button*, 5> kButtons{back_, forward_, reload_, home_,
                                               avatar_};
  auto* button = *base::ranges::find(kButtons, id, &views::Button::tag);
  DCHECK(button);
  button->SetEnabled(enabled);
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

gfx::Size ToolbarView::CalculatePreferredSize() const {
  gfx::Size size;
  switch (display_mode_) {
    case DisplayMode::CUSTOM_TAB:
      size = custom_tab_bar_->GetPreferredSize();
      break;
    case DisplayMode::LOCATION:
      size = location_bar_->GetPreferredSize();
      break;
    case DisplayMode::NORMAL:
      size = container_view_->GetPreferredSize();
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/985909), we
      // cap the height at the size of known child views (location bar and back
      // button) plus margins.
      // TODO(crbug.com/1033627): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->GetVisible()) {
        const int max_height =
            std::max(location_bar_->GetPreferredSize().height(),
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
    case DisplayMode::CUSTOM_TAB:
      size = custom_tab_bar_->GetMinimumSize();
      break;
    case DisplayMode::LOCATION:
      size = location_bar_->GetMinimumSize();
      break;
    case DisplayMode::NORMAL:
      size = container_view_->GetMinimumSize();
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/985909), we
      // cap the height at the size of known child views (location bar and back
      // button) plus margins.
      // TODO(crbug.com/1033627): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->GetVisible()) {
        const int max_height =
            std::max(location_bar_->GetMinimumSize().height(),
                     back_->GetMinimumSize().height()) +
            layout_manager_->interior_margin().height();
        size.SetToMin({size.width(), max_height});
      }
  }
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (!initialized_)
    return;

  // The container view should be the exact same size/position as ToolbarView.
  container_view_->SetSize(size());

  if (features::IsChromeRefresh2023()) {
    // The background views should be behind the top-left and top-right corners
    // of the container_view_.
    const int corner_radius = GetLayoutConstant(TOOLBAR_CORNER_RADIUS);
    background_view_left_->SetBounds(0, 0, corner_radius, corner_radius);
    background_view_right_->SetBounds(width() - corner_radius, 0, corner_radius,
                                      corner_radius);
  }

  if (display_mode_ == DisplayMode::CUSTOM_TAB) {
    custom_tab_bar_->SetBounds(0, 0, width(),
                               custom_tab_bar_->GetPreferredSize().height());
    location_bar_->SetVisible(false);
    return;
  }

  if (display_mode_ == DisplayMode::NORMAL) {
    LayoutCommon();

    if (features::IsChromeRefresh2023()) {
      UpdateClipPath();
    }
  }

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  AccessiblePaneView::Layout();
}

void ToolbarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  if (!initialized_)
    return;

  if (display_mode_ == DisplayMode::NORMAL)
    LoadImages();

  SchedulePaint();
}

void ToolbarView::UpdateClipPath() {
  const int corner_radius = GetLayoutConstant(TOOLBAR_CORNER_RADIUS);
  SkPath path;
  const gfx::Rect local_bounds = GetLocalBounds();
  path.moveTo(0, local_bounds.height());
  path.lineTo(0, corner_radius);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, corner_radius, 0);
  path.lineTo(local_bounds.width() - corner_radius, 0);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, local_bounds.width(), corner_radius);
  path.lineTo(local_bounds.width(), local_bounds.height());
  path.lineTo(0, local_bounds.height());
  container_view_->SetClipPath(path);
}

void ToolbarView::ActiveStateChanged() {
  background_view_left_->SchedulePaint();
  background_view_right_->SchedulePaint();
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view && (focused_view->GetID() == VIEW_ID_OMNIBOX))
    return false;  // Let the omnibox handle all accelerator events.
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

void ToolbarView::ChildPreferredSizeChanged(views::View* child) {
  InvalidateLayout();
  if (size() != GetPreferredSize())
    PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar - and
// also so that it selects all content in the location bar.
views::View* ToolbarView::GetDefaultFocusableChild() {
  return location_bar_;
}

void ToolbarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
}

void ToolbarView::InitLayout() {
  const int default_margin = GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN);
  // TODO(dfried): rename this constant.
  const int location_bar_margin = GetLayoutConstant(TOOLBAR_STANDARD_SPACING);

  const views::FlexSpecification account_container_flex_rule =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1);
  const views::FlexSpecification location_bar_flex_rule =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2);
  constexpr int kSidePanelFlexOrder = 3;
  constexpr int kExtensionsFlexOrder = 4;

  layout_manager_ =
      container_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, default_margin));

  location_bar_->SetProperty(views::kFlexBehaviorKey, location_bar_flex_rule);
  location_bar_->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, location_bar_margin));

  if (extensions_container_) {
    const views::FlexSpecification extensions_flex_rule =
        views::FlexSpecification(
            extensions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kExtensionsFlexOrder);

    extensions_container_->SetProperty(views::kFlexBehaviorKey,
                                       extensions_flex_rule);
  }

  if (side_panel_container_) {
    const views::FlexSpecification side_panel_flex_rule =
        views::FlexSpecification(
            side_panel_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kSidePanelFlexOrder);

    side_panel_container_->SetProperty(views::kFlexBehaviorKey,
                                       side_panel_flex_rule);
  }

  if (toolbar_divider_) {
    toolbar_divider_->SetProperty(views::kMarginsKey,
                                  gfx::Insets::VH(0, kToolbarDividerSpacing));
  }

  LayoutCommon();
}

void ToolbarView::LayoutCommon() {
  DCHECK(display_mode_ == DisplayMode::NORMAL);

  gfx::Insets interior_margin =
      GetLayoutInsets(browser_view_->webui_tab_strip()
                          ? LayoutInset::WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN
                          : LayoutInset::TOOLBAR_INTERIOR_MARGIN);

  if (features::IsChromeRefresh2023() && !browser_view_->webui_tab_strip()) {
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
    toolbar_divider_->SetVisible(extensions_container_->GetVisible());
    const SkColor toolbar_extension_separator_color =
        GetColorProvider()->GetColor(kColorToolbarExtensionSeparatorEnabled);
    toolbar_divider_->SetBackground(views::CreateRoundedRectBackground(
        toolbar_extension_separator_color, kToolbarDividerCornerRadius));
  }
  // Cast button visibility is controlled externally.
}

// AppMenuIconController::Delegate:
void ToolbarView::UpdateTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  // There's no app menu in tabless windows.
  if (!app_menu_button_)
    return;

  std::u16string accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (type_and_severity.type ==
      AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    accname_app = l10n_util::GetStringFUTF16(
        IDS_ACCNAME_APP_UPGRADE_RECOMMENDED, accname_app);
  }
  app_menu_button_->SetAccessibleName(accname_app);
  app_menu_button_->SetTypeAndSeverity(type_and_severity);
}

SkColor ToolbarView::GetDefaultColorForSeverity(
    AppMenuIconController::Severity severity) const {
  ui::ColorId color_id;
  switch (severity) {
    case AppMenuIconController::Severity::NONE:
      return GetColorProvider()->GetColor(kColorToolbarButtonIcon);
    case AppMenuIconController::Severity::LOW:
      color_id = kColorAppMenuHighlightSeverityLow;
      break;
    case AppMenuIconController::Severity::MEDIUM:
      color_id = kColorAppMenuHighlightSeverityMedium;
      break;
    case AppMenuIconController::Severity::HIGH:
      color_id = kColorAppMenuHighlightSeverityHigh;
      break;
  }
  return GetColorProvider()->GetColor(color_id);
}

ExtensionsToolbarContainer* ToolbarView::GetExtensionsToolbarContainer() {
  return extensions_container_;
}

gfx::Size ToolbarView::GetToolbarButtonSize() const {
  const int size = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  return gfx::Size(size, size);
}

views::View* ToolbarView::GetDefaultExtensionDialogAnchorView() {
  if (extensions_container_)
    return extensions_container_->GetExtensionsButton();
  return GetAppMenuButton();
}

PageActionIconView* ToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return location_bar()->page_action_icon_controller()->GetIconView(type);
}

AppMenuButton* ToolbarView::GetAppMenuButton() {
  if (app_menu_button_)
    return app_menu_button_;

  return custom_tab_bar_ ? custom_tab_bar_->custom_tab_menu_button() : nullptr;
}

gfx::Rect ToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return gfx::Rect();

  if (!location_bar_->IsDrawn())
    return gfx::Rect();

  gfx::Rect bounds =
      location_bar_->ConvertRectToWidget(location_bar_->GetLocalBounds());
  return gfx::Rect(bounds.x(), bounds.bottom(), bounds.width(),
                   contents_bottom - bounds.bottom());
}

void ToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* ToolbarView::GetAnchorView(PageActionIconType type) {
  return location_bar_;
}

void ToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  location_bar_->page_action_icon_controller()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* ToolbarView::GetAvatarToolbarButton() {
  if (avatar_)
    return avatar_;

  return nullptr;
}

ToolbarButton* ToolbarView::GetBackButton() {
  return back_;
}

ReloadButton* ToolbarView::GetReloadButton() {
  return reload_;
}

IntentChipButton* ToolbarView::GetIntentChipButton() {
  return location_bar()->intent_chip();
}

DownloadToolbarButtonView* ToolbarView::GetDownloadButton() {
  return download_button();
}

BrowserRootView::DropIndex ToolbarView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return {browser_->tab_strip_model()->active_index(), false};
}

BrowserRootView::DropTarget* ToolbarView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  return HitTestPoint(loc_in_local_coords) ? this : nullptr;
}

views::View* ToolbarView::GetViewForDrop() {
  return this;
}

void ToolbarView::OnChromeLabsPrefChanged() {
  chrome_labs_button_->SetVisible(show_chrome_labs_button_.GetValue());
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      chrome_labs_button_->GetVisible()
          ? IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_ADDED_BY_ENTERPRISE_POLICY
          : IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_REMOVED_BY_ENTERPRISE_POLICY));
}

void ToolbarView::LoadImages() {
  DCHECK_EQ(display_mode_, DisplayMode::NORMAL);

  if (extensions_container_)
    extensions_container_->UpdateAllIcons();
}

void ToolbarView::OnShowHomeButtonChanged() {
  home_->SetVisible(show_home_button_.GetValue());
  Layout();
  SchedulePaint();
}

void ToolbarView::OnTouchUiChanged() {
  if (display_mode_ == DisplayMode::NORMAL) {
    // Update the internal margins for touch layout.
    // TODO(dfried): I think we can do better than this by making the touch UI
    // code cleaner.
    const int default_margin = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
    const int location_bar_margin = GetLayoutConstant(TOOLBAR_STANDARD_SPACING);
    layout_manager_->SetDefault(views::kMarginsKey,
                                gfx::Insets::VH(0, default_margin));
    location_bar_->SetProperty(views::kMarginsKey,
                               gfx::Insets::VH(0, location_bar_margin));

    LoadImages();
    PreferredSizeChanged();
  }
}

BEGIN_METADATA(ToolbarView, views::AccessiblePaneView)
ADD_READONLY_PROPERTY_METADATA(bool, AppMenuFocused)
END_METADATA
