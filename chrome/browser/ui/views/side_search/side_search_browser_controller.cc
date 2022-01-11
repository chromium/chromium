// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kSidePanelWidth = 380;
constexpr int kDefaultTouchableIconSize = 24;

// Below are hardcoded color constants for the side panel. This is a UX decision
// to ensure that the colors align with the tier 2 Google SRP which only
// supports light mode. These are not intended to change to match the light/dark
// system setting or custom theme colors.

// White background to match the Google SRP.
constexpr SkColor kHeaderBackgroundColor = SK_ColorWHITE;

// Default light mode icon color for controls.
constexpr SkColor kIconColor = gfx::kGoogleGrey700;

// Default light mode separator color.
constexpr SkColor kSeparatorColor = gfx::kGoogleGrey300;

// Base header button class. Responds appropriately to touch ui changes.
class HeaderButton : public views::ImageButton {
 public:
  METADATA_HEADER(HeaderButton);
  HeaderButton(const gfx::VectorIcon& icon, base::RepeatingClosure callback)
      : ImageButton(std::move(callback)), icon_(icon) {
    views::ConfigureVectorImageButton(this);

    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_CLOSE_BUTTON_MARGIN))));

    UpdateIcon();
  }
  ~HeaderButton() override = default;

  void UpdateIcon() {
    const int icon_size =
        ui::TouchUiController::Get()->touch_ui()
            ? kDefaultTouchableIconSize
            : ChromeLayoutProvider::Get()->GetDistanceMetric(
                  ChromeDistanceMetric::
                      DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
    views::SetImageFromVectorIconWithColor(this, icon_, icon_size, kIconColor);
  }

 private:
  const gfx::VectorIcon& icon_;
};

BEGIN_METADATA(HeaderButton, views::ImageButton)
END_METADATA

// Header view used to house the close control at the top of the side panel.
class HeaderView : public views::View {
 public:
  METADATA_HEADER(HeaderView);
  explicit HeaderView(base::RepeatingClosure callback, Browser* browser)
      : close_button_(AddChildView(
            std::make_unique<HeaderButton>(vector_icons::kCloseIcon,
                                           std::move(callback)))),
        layout_(SetLayoutManager(std::make_unique<views::BoxLayout>())) {
    views::InstallCircleHighlightPathGenerator(close_button_);
    close_button_->SetID(SideSearchBrowserController::SideSearchViewID::
                             VIEW_ID_SIDE_PANEL_CLOSE_BUTTON);
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_SEARCH_CLOSE_BUTTON));
    close_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_SEARCH_CLOSE_BUTTON));

    if (base::FeatureList::IsEnabled(features::kSideSearchFeedback)) {
      base::RepeatingClosure feedback_callback = base::BindRepeating(
          [](Browser* browser) {
            chrome::ShowFeedbackPage(
                browser, chrome::FeedbackSource::kFeedbackSourceChromeLabs,
                std::string() /* description_template */,
                l10n_util::GetStringFUTF8(
                    IDS_CHROMELABS_SEND_FEEDBACK_DESCRIPTION_PLACEHOLDER,
                    l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_SEARCH_TOOL)),
                std::string("chrome-labs-side-search"),
                std::string() /* extra_diagnostics */);
          },
          browser);

      feedback_button_ =
          AddChildViewAt(std::make_unique<HeaderButton>(
                             kSubmitFeedbackIcon, std::move(feedback_callback)),
                         0);
      feedback_button_->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_SEARCH_FEEDBACK_BUTTON));
      feedback_button_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_SEARCH_FEEDBACK_BUTTON));
    }

    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));
    SetBackground(views::CreateSolidBackground(kHeaderBackgroundColor));
    layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
    UpdateSpacing();
  }
  ~HeaderView() override = default;

 private:
  void UpdateSpacing() {
    close_button_->UpdateIcon();

    if (feedback_button_)
      feedback_button_->UpdateIcon();

    layout_->set_inside_border_insets(
        GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN));
  }

  HeaderButton* const close_button_;
  HeaderButton* feedback_button_ = nullptr;
  views::BoxLayout* const layout_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&HeaderView::UpdateSpacing,
                              base::Unretained(this)));
};

BEGIN_METADATA(HeaderView, views::View)
END_METADATA

std::unique_ptr<views::Separator> CreateSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(kSeparatorColor);
  return separator;
}

views::WebView* ConfigureSidePanel(views::View* side_panel,
                                   Profile* profile,
                                   Browser* browser,
                                   base::RepeatingClosure callback) {
  // BrowserViewLayout will layout the SidePanel to match the height of the
  // content area.
  side_panel->SetPreferredSize(gfx::Size(kSidePanelWidth, 1));

  auto container = std::make_unique<views::FlexLayoutView>();
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  container->AddChildView(
      std::make_unique<HeaderView>(std::move(callback), browser));
  container->AddChildView(CreateSeparator());

  // The WebView will fill the remaining space after the header view has been
  // laid out.
  auto* web_view =
      container->AddChildView(std::make_unique<views::WebView>(profile));
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  side_panel->AddChildView(std::move(container));

  // The side panel should not start visible to avoid having it participate in
  // initial layout calculations. Its visibility state will be updated later on
  // in UpdateSidePanel().
  side_panel->SetVisible(false);

  return web_view;
}

}  // namespace

SideSearchBrowserController::SideSearchBrowserController(
    SidePanel* side_panel,
    BrowserView* browser_view)
    : side_panel_(side_panel),
      browser_view_(browser_view),
      web_view_(ConfigureSidePanel(
          side_panel,
          browser_view_->GetProfile(),
          browser_view_->browser(),
          base::BindRepeating(
              &SideSearchBrowserController::SidePanelCloseButtonPressed,
              base::Unretained(this)))),
      focus_tracker_(side_panel_, browser_view_->GetFocusManager()) {
  web_view_visibility_subscription_ =
      web_view_->AddVisibleChangedCallback(base::BindRepeating(
          &SideSearchBrowserController::OnWebViewVisibilityChanged,
          base::Unretained(this)));

  browser_view_observation_.Observe(browser_view_);
  UpdateSidePanelForContents(browser_view_->GetActiveWebContents(), nullptr);
}

SideSearchBrowserController::~SideSearchBrowserController() {
  Observe(nullptr);
}

bool SideSearchBrowserController::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, browser_view_->GetFocusManager());
}

content::WebContents* SideSearchBrowserController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return browser_view_->browser()->OpenURL(params);
}

void SideSearchBrowserController::SidePanelAvailabilityChanged(
    bool should_close) {
  if (should_close) {
    CloseSidePanel();
  } else {
    UpdateSidePanel();
  }
}

void SideSearchBrowserController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // The toggled state of the side panel for this tab contents should be reset
  // when landing on a page that should not show the side panel in the
  // state-per-tab mode (e.g. NTP, Google home page etc). This will prevent the
  // side panel from reopening automatically after the tab next encounters a
  // page where the side panel can be shown.
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)) {
    auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
        navigation_handle->GetWebContents());
    if (GetSidePanelToggledOpen() &&
        !tab_contents_helper->CanShowSidePanelForCommittedNavigation()) {
      CloseSidePanel();
      return;
    }
  }

  // We need to update the side panel state in response to navigations to catch
  // cases where the user navigates to a page that should have the side panel
  // hidden (e.g. the Google home page).
  UpdateSidePanel();
}

void SideSearchBrowserController::OnViewAddedToWidget(
    views::View* observed_view) {
  DCHECK_EQ(browser_view_, observed_view);
  focus_tracker_.SetFocusManager(browser_view_->GetFocusManager());
}

void SideSearchBrowserController::UpdateSidePanelForContents(
    content::WebContents* new_contents,
    content::WebContents* old_contents) {
  // Ensure that the controller acts as the delegate only to the currently
  // active contents.
  if (old_contents) {
    SideSearchTabContentsHelper::FromWebContents(old_contents)
        ->SetDelegate(nullptr);
  }
  if (new_contents) {
    SideSearchTabContentsHelper::FromWebContents(new_contents)
        ->SetDelegate(weak_factory_.GetWeakPtr());
  }

  Observe(new_contents);

  // Update the state of the side panel to catch cases where we switch to a tab
  // where the panel should be hidden (or vise versa).
  UpdateSidePanel();
}

std::unique_ptr<ToolbarButton>
SideSearchBrowserController::CreateToolbarButton() {
  auto toolbar_button = std::make_unique<ToolbarButton>();
  toolbar_button->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ACCNAME_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));
  toolbar_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON));
  toolbar_button->SetProperty(views::kElementIdentifierKey,
                              kSideSearchButtonElementId);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  toolbar_button->SetVectorIcon(kGoogleGLogoMonochromeIcon);
#else
  toolbar_button->SetVectorIcon(kWebIcon);
#endif

  toolbar_button->SetCallback(
      base::BindRepeating(&SideSearchBrowserController::SidePanelButtonPressed,
                          base::Unretained(this)));
  toolbar_button->SetVisible(false);
  toolbar_button->SetEnabled(true);

  toolbar_button_ = toolbar_button.get();
  return toolbar_button;
}

bool SideSearchBrowserController::GetSidePanelToggledOpen() const {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)) {
    auto* active_contents = browser_view_->GetActiveWebContents();
    return active_contents
               ? SideSearchTabContentsHelper::FromWebContents(active_contents)
                     ->toggled_open()
               : false;
  }
  return toggled_open_;
}

void SideSearchBrowserController::SidePanelButtonPressed() {
  if (GetSidePanelToggledOpen())
    CloseSidePanel(SideSearchCloseActionType::kTapOnSideSearchToolbarButton);
  else
    OpenSidePanel();
}

void SideSearchBrowserController::SidePanelCloseButtonPressed() {
  CloseSidePanel(SideSearchCloseActionType::kTapOnSideSearchCloseButton);
}

void SideSearchBrowserController::OpenSidePanel() {
  RecordSideSearchOpenAction(
      SideSearchOpenActionType::kTapOnSideSearchToolbarButton);
  // Close the Side Search IPH if it is showing.
  FeaturePromoControllerViews* controller =
      FeaturePromoControllerViews::GetForView(toolbar_button_);
  if (controller)
    controller->CloseBubble(feature_engagement::kIPHSideSearchFeature);
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      browser_view_->GetProfile());
  if (tracker)
    tracker->NotifyEvent(feature_engagement::events::kSideSearchOpened);

  SetSidePanelToggledOpen(true);
  UpdateSidePanel();

  // After showing the side panel if the web_view_ is visible request focus.
  if (web_view_->GetVisible())
    web_view_->web_contents()->Focus();
}

void SideSearchBrowserController::CloseSidePanel(
    absl::optional<SideSearchCloseActionType> action) {
  if (action)
    RecordSideSearchCloseAction(action.value());

  focus_tracker_.FocusLastFocusedExternalView();
  SetSidePanelToggledOpen(false);
  UpdateSidePanel();

  if (base::FeatureList::IsEnabled(features::kSideSearchClearCacheWhenClosed)) {
    // If per tab state is enabled only clear the side contents for the
    // currently active tab.
    base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)
        ? ClearSideContentsCacheForActiveTab()
        : ClearSideContentsCacheForBrowser();
  }
}

void SideSearchBrowserController::ClearSideContentsCacheForBrowser() {
  web_view_->SetWebContents(nullptr);

  // Notify the tab helpers that their side panel contentes can be cleared away.
  TabStripModel* tab_strip_model = browser_view_->browser()->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    SideSearchTabContentsHelper::FromWebContents(
        tab_strip_model->GetWebContentsAt(i))
        ->ClearSidePanelContents();
  }
}

void SideSearchBrowserController::ClearSideContentsCacheForActiveTab() {
  web_view_->SetWebContents(nullptr);

  if (auto* active_contents = browser_view_->GetActiveWebContents()) {
    SideSearchTabContentsHelper::FromWebContents(active_contents)
        ->ClearSidePanelContents();
  }
}

void SideSearchBrowserController::SetSidePanelToggledOpen(bool toggled_open) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab)) {
    if (auto* active_contents = browser_view_->GetActiveWebContents()) {
      SideSearchTabContentsHelper::FromWebContents(active_contents)
          ->set_toggled_open(toggled_open);
      side_search::MaybeSaveSideSearchTabSessionData(active_contents);
    }
  } else {
    toggled_open_ = toggled_open;
  }
}

void SideSearchBrowserController::UpdateSidePanel() {
  auto* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents) {
    // Ensure we reset the `web_view_`'s hosted side contents when the active
    // tab contents is null to cover cases such as the tab being moved to
    // another window. This is needed as the WebView's destructor will not be
    // invoked until both the remove model update is fired in this window and
    // the add model update is fired in the destination window.
    web_view_->SetWebContents(nullptr);
    return;
  }

  // Switch the WebContents currently in the windows side panel to the
  // WebContents associated with the active tab.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);

  const bool can_show_side_panel_for_page =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation();
  const bool will_show_side_panel =
      can_show_side_panel_for_page && GetSidePanelToggledOpen();

  // The side panel contents will be created if it does not already exist.
  web_view_->SetWebContents(will_show_side_panel
                                ? tab_contents_helper->GetSidePanelContents()
                                : nullptr);
  side_panel_->SetVisible(will_show_side_panel);
  toolbar_button_->SetHighlighted(will_show_side_panel);
  toolbar_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      will_show_side_panel
          ? IDS_ACCNAME_SIDE_SEARCH_TOOLBAR_BUTTON_ACTIVATED
          : IDS_ACCNAME_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));

  // The toolbar button should remain visible in the toolbar as a side panel can
  // be shown for the active tab.
  if (toolbar_button_->GetVisible() != can_show_side_panel_for_page) {
    toolbar_button_->SetVisible(can_show_side_panel_for_page);
    RecordSideSearchAvailabilityChanged(
        can_show_side_panel_for_page
            ? SideSearchAvailabilityChangeType::kBecomeAvailable
            : SideSearchAvailabilityChangeType::kBecomeUnavailable);
  }

  // Once the anchor element is visible, maybe show promo.
  if (can_show_side_panel_for_page &&
      tab_contents_helper->returned_to_previous_srp()) {
    browser_view_->feature_promo_controller()->MaybeShowPromo(
        feature_engagement::kIPHSideSearchFeature);
  }

  browser_view_->InvalidateLayout();
}

void SideSearchBrowserController::OnWebViewVisibilityChanged() {
  // After the web_view_ becomes visible have the side contents request focus.
  // We need to do this in the web_view_'s visibility changed listener as the
  // web_view_'s visibility state could be updated by the layout manager during
  // layout and we should not do this until the web_view_ is visible. Layout is
  // invalidated when we call UpdateSidePanel() but is scheduled asynchronously
  // by the hosting Widget.
  if (web_view_->GetVisible())
    web_view_->web_contents()->Focus();
}
