// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/url_formatter/elide_url.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kDefaultTouchableIconSize = 24;

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

  // views::ImageButton:
  void OnThemeChanged() override {
    ImageButton::OnThemeChanged();
    views::InkDrop::Get(this)->SetBaseColor(
        GetColorProvider()->GetColor(ui::kColorIcon));
  }

  void UpdateIcon() {
    const int icon_size =
        ui::TouchUiController::Get()->touch_ui()
            ? kDefaultTouchableIconSize
            : ChromeLayoutProvider::Get()->GetDistanceMetric(
                  ChromeDistanceMetric::
                      DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
    SetImageModel(Button::STATE_NORMAL, ui::ImageModel::FromVectorIcon(
                                            *icon_, ui::kColorIcon, icon_size));
    SetImageModel(Button::STATE_DISABLED,
                  ui::ImageModel::FromVectorIcon(*icon_, ui::kColorIconDisabled,
                                                 icon_size));
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(HeaderButton, views::ImageButton)
END_METADATA

// A view that tracks the icon image of the current DSE.
class DseImageView : public views::ImageView {
 public:
  METADATA_HEADER(DseImageView);
  explicit DseImageView(Browser* browser)
      : browser_(browser),
        icon_changed_subscription_(
            DefaultSearchIconSource::GetOrCreateForBrowser(browser)
                ->RegisterIconChangedSubscription(
                    base::BindRepeating(&DseImageView::UpdateIconImage,
                                        base::Unretained(this)))) {
    SetFlipCanvasOnPaintForRTLUI(false);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(0, views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_RELATED_CONTROL_VERTICAL))));
    UpdateIconImage();
  }
  ~DseImageView() override = default;

  void UpdateIconImage() {
    // Attempt to get the default search engine's favicon.
    auto* default_search_icon_source =
        DefaultSearchIconSource::GetOrCreateForBrowser(browser_);
    auto icon_image = default_search_icon_source->GetIconImage();

    // Use the DSE's icon image if available.
    if (!icon_image.IsEmpty()) {
      SetImage(default_search_icon_source->GetIconImage());
      return;
    }

    // If the icon image is empty use kSearchIcon as a default. It is not
    // guaranteed that the FaviconService will return a favicon for the search
    // provider.
    const int icon_size =
        ui::TouchUiController::Get()->touch_ui()
            ? kDefaultTouchableIconSize
            : ChromeLayoutProvider::Get()->GetDistanceMetric(
                  ChromeDistanceMetric::
                      DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
    SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kSearchIcon,
                                            ui::kColorIcon, icon_size));
  }

 private:
  const raw_ptr<Browser> browser_;

  // Subscription to change notifications to the default search icon source.
  base::CallbackListSubscription icon_changed_subscription_;
};

BEGIN_METADATA(DseImageView, views::ImageView)
END_METADATA

// Header view for the side search side panel. The structure is as follows.
//  ___________________________________________________________________________
// | dse_image_view | simple_site_name        | feedback_button | close_button |
// |
//  ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
// The image view and buttons are fixed at their preferred size. The simple site
// name label is configured to consume the remaining horizontal space.
class HeaderView : public views::View {
 public:
  METADATA_HEADER(HeaderView);
  HeaderView(base::RepeatingClosure callback, Browser* browser)
      : layout_(SetLayoutManager(std::make_unique<views::FlexLayout>())) {
    constexpr int kHeaderHeight = 44;
    SetPreferredSize(gfx::Size(0, kHeaderHeight));

    constexpr int kHorizontalMargin = 8;
    layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetInteriorMargin(gfx::Insets::VH(0, kHorizontalMargin));

    dse_image_view_ = AddChildView(std::make_unique<DseImageView>(browser));
    dse_image_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));

    title_label_ = AddChildView(std::make_unique<views::Label>());
    title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label_->SetTextContext(CONTEXT_SIDE_PANEL_TITLE);
    title_label_->SetTextStyle(views::style::STYLE_PRIMARY);
    title_label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));

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
      feedback_button_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
    }

    close_button_ = AddChildView(std::make_unique<HeaderButton>(
        vector_icons::kCloseIcon, std::move(callback)));
    views::InstallCircleHighlightPathGenerator(close_button_);
    close_button_->SetProperty(views::kElementIdentifierKey,
                               kSidePanelCloseButtonElementId);
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_SEARCH_CLOSE_BUTTON));
    close_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_SEARCH_CLOSE_BUTTON));
    close_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));

    // Ensure the header view's containing view keeps its vertical size at the
    // preferred size when laying out the side panel. The side panel does this
    // using a flex layout so we need to ensure we set the correct flex
    // behavior.
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));
    SetBackground(
        views::CreateThemedSolidBackground(ui::kColorDialogBackground));
    UpdateSpacing();
  }
  ~HeaderView() override = default;

  views::Label* get_title_label() { return title_label_; }

 private:
  // Updates the toolbar insets which may change as we enter / leave touch mode.
  // Icons are also updated to give them the opportunity to resize and adjust
  // their insets.
  void UpdateSpacing() {
    dse_image_view_->UpdateIconImage();
    if (feedback_button_)
      feedback_button_->UpdateIcon();
    close_button_->UpdateIcon();
  }

  raw_ptr<DseImageView> dse_image_view_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<HeaderButton> feedback_button_ = nullptr;
  raw_ptr<HeaderButton> close_button_ = nullptr;

  raw_ptr<views::FlexLayout> const layout_;

  // Used to listen for when the UI enters / leaves touch mode.
  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&HeaderView::UpdateSpacing,
                              base::Unretained(this)));
};

BEGIN_METADATA(HeaderView, views::View)
END_METADATA

}  // namespace

SideSearchBrowserController::SideSearchBrowserController(
    SidePanel* side_panel,
    BrowserView* browser_view)
    : side_panel_(side_panel),
      browser_view_(browser_view),
      focus_tracker_(side_panel_, browser_view_->GetFocusManager()) {
  auto container = std::make_unique<views::FlexLayoutView>();
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  auto* header_view = container->AddChildView(std::make_unique<HeaderView>(
      base::BindRepeating(
          &SideSearchBrowserController::SidePanelCloseButtonPressed,
          base::Unretained(this)),
      browser_view_->browser()));
  title_label_ = header_view->get_title_label();
  container->AddChildView(std::make_unique<views::Separator>());

  // The WebView will fill the remaining space after the header view has been
  // laid out.
  web_view_ = container->AddChildView(
      std::make_unique<views::WebView>(browser_view_->GetProfile()));
  web_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  web_view_->SetBackground(views::CreateThemedSolidBackground(kColorToolbar));

  side_panel_->AddChildView(std::move(container));

  // The side panel should not start visible to avoid having it participate in
  // initial layout calculations. Its visibility state will be updated later on
  // in UpdateSidePanel().
  side_panel_->SetVisible(false);

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
  // when landing on a page that should not show the side panel (e.g. NTP,
  // Google home page etc). This will prevent the side panel from reopening
  // automatically after the tab next encounters a page where the side panel can
  // be shown.
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      navigation_handle->GetWebContents());
  if (!tab_contents_helper)
    return;

  if (GetSidePanelToggledOpen() &&
      !tab_contents_helper->CanShowSidePanelForCommittedNavigation()) {
    CloseSidePanel();
    return;
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
    auto* helper = SideSearchTabContentsHelper::FromWebContents(old_contents);
    if (helper)
      helper->SetDelegate(nullptr);
  }
  if (new_contents) {
    auto* helper = SideSearchTabContentsHelper::FromWebContents(new_contents);
    if (helper)
      helper->SetDelegate(weak_factory_.GetWeakPtr());
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
  toolbar_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));
  toolbar_button->SetProperty(views::kElementIdentifierKey,
                              kSideSearchButtonElementId);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  toolbar_button->SetVectorIcon(vector_icons::kGoogleGLogoMonochromeIcon);
#else
  toolbar_button->SetVectorIcon(kWebIcon);
#endif

  toolbar_button->SetCallback(base::BindRepeating(
      &SideSearchBrowserController::ToggleSidePanel, base::Unretained(this)));
  toolbar_button->SetVisible(false);
  toolbar_button->SetEnabled(true);

  toolbar_button_ = toolbar_button.get();
  return toolbar_button;
}

void SideSearchBrowserController::ToggleSidePanel() {
  if (GetSidePanelToggledOpen())
    CloseSidePanel(SideSearchCloseActionType::kTapOnSideSearchToolbarButton);
  else
    OpenSidePanel();
}

bool SideSearchBrowserController::GetSidePanelToggledOpen() const {
  auto* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return false;

  const auto* helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  return helper ? helper->toggled_open() : false;
}

void SideSearchBrowserController::SidePanelCloseButtonPressed() {
  CloseSidePanel(SideSearchCloseActionType::kTapOnSideSearchCloseButton);
  browser_view_->RightAlignedSidePanelWasClosed();
}

void SideSearchBrowserController::OpenSidePanel() {
  RecordSideSearchOpenAction(
      SideSearchOpenActionType::kTapOnSideSearchToolbarButton);
  RecordSidePanelOpenedMetrics();

  shown_via_entrypoint_ = true;

  // Close the Side Search IPH if it is showing.
  browser_view_->CloseFeaturePromo(feature_engagement::kIPHSideSearchFeature);
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      browser_view_->GetProfile());
  if (tracker)
    tracker->NotifyEvent(feature_engagement::events::kSideSearchOpened);

  SetSidePanelToggledOpen(true);
  UpdateSidePanel();

  // After showing the side panel if the web_view_ is visible request focus.
  if (web_view_->GetVisible() && web_view_->web_contents())
    web_view_->web_contents()->Focus();
}

void SideSearchBrowserController::CloseSidePanel(
    absl::optional<SideSearchCloseActionType> action) {
  if (action)
    RecordSideSearchCloseAction(action.value());

  focus_tracker_.FocusLastFocusedExternalView();
  SetSidePanelToggledOpen(false);
  UpdateSidePanel();

  // Clear the side contents for the currently active tab.
  ClearSideContentsCacheForActiveTab();
}

void SideSearchBrowserController::ClobberAllInCurrentBrowser() {
  auto* tab_strip_model = browser_view_->browser()->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    auto* helper = SideSearchTabContentsHelper::FromWebContents(
        tab_strip_model->GetWebContentsAt(i));
    if (helper)
      helper->set_toggled_open(false);
  }
}

void SideSearchBrowserController::ClearSideContentsCacheForActiveTab() {
  web_view_->SetWebContents(nullptr);

  if (auto* active_contents = browser_view_->GetActiveWebContents()) {
    auto* helper =
        SideSearchTabContentsHelper::FromWebContents(active_contents);
    if (helper)
      helper->ClearSidePanelContents();
  }
}

void SideSearchBrowserController::SetSidePanelToggledOpen(bool toggled_open) {
  if (auto* active_contents = browser_view_->GetActiveWebContents()) {
    auto* helper =
        SideSearchTabContentsHelper::FromWebContents(active_contents);
    if (helper) {
      helper->set_toggled_open(toggled_open);
      side_search::MaybeSaveSideSearchTabSessionData(active_contents);
    }
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

  // Early return if the tab helper for the active tab is not defined
  // (crbug.com/1307908).
  if (!tab_contents_helper)
    return;

  const bool can_show_side_panel_for_page =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation();
  const bool will_show_side_panel =
      can_show_side_panel_for_page && GetSidePanelToggledOpen();

  // When side search is shown we only need to close other side panels for the
  // basic clobbering experience. The improved experience leverages a
  // SidePanelVisibilityController on the browser view.
  if (base::FeatureList::IsEnabled(features::kSideSearchDSESupport) &&
      !base::FeatureList::IsEnabled(features::kSidePanelImprovedClobbering) &&
      will_show_side_panel) {
    browser_view_->CloseOpenRightAlignedSidePanel(/*exclude_lens=*/false,
                                                  /*exclude_side_search=*/true);
  }

  // The side panel contents will be created if it does not already exist.
  const auto* previous_hosted_contents = web_view_->web_contents();
  web_view_->SetWebContents(will_show_side_panel
                                ? tab_contents_helper->GetSidePanelContents()
                                : nullptr);

  // Log time shown metrics whenever a new contents is hosted in the side panel.
  if (previous_hosted_contents != web_view_->web_contents()) {
    // If we were hosting a side panel contents, log its open duration.
    if (previous_hosted_contents) {
      DCHECK(side_panel_shown_timer_);
      RecordSideSearchSidePanelTimeShown(shown_via_entrypoint_,
                                         side_panel_shown_timer_->Elapsed());
    }

    // Reset the `shown_via_entrypoint_` flag only if we were previously hosting
    // a side panel contents. Do this to avoid prematurely resetting the flag
    // when the side panel is first shown before the shown duration is logged.
    if (previous_hosted_contents)
      shown_via_entrypoint_ = false;

    // If hosting a new side panel contents start a new timer. If no longer
    // hosting a side panel contents clear the timer.
    DCHECK_EQ(will_show_side_panel, !!web_view_->web_contents());
    if (web_view_->web_contents()) {
      side_panel_shown_timer_ = base::ElapsedTimer();
    } else {
      side_panel_shown_timer_.reset();
    }
  }

  // Update the side panel header title text if necessary
  if (auto last_search_url = tab_contents_helper->last_search_url()) {
    title_label_->SetText(
        url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
            last_search_url.value()));
  }

  side_panel_->SetVisible(will_show_side_panel);

  // Update the side panel entrypoints - either the page action or the toolbar
  // button.
  // TODO(tluk): Split the entrypoint implementations out into a separate class.
  browser_view_->UpdatePageActionIcon(PageActionIconType::kSideSearch);

  if (toolbar_button_) {
    toolbar_button_->SetHighlighted(will_show_side_panel);
    toolbar_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        will_show_side_panel
            ? IDS_ACCNAME_SIDE_SEARCH_TOOLBAR_BUTTON_ACTIVATED
            : IDS_ACCNAME_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));
    toolbar_button_->SetTooltipText(l10n_util::GetStringUTF16(
        will_show_side_panel
            ? IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_ACTIVATED
            : IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));

    // The toolbar button should remain visible in the toolbar as a side panel
    // can be shown for the active tab.
    if (toolbar_button_->GetVisible() != can_show_side_panel_for_page)
      toolbar_button_->SetVisible(can_show_side_panel_for_page);
  }

  if (was_side_panel_available_for_page_ != can_show_side_panel_for_page) {
    RecordSideSearchAvailabilityChanged(
        can_show_side_panel_for_page
            ? SideSearchAvailabilityChangeType::kBecomeAvailable
            : SideSearchAvailabilityChangeType::kBecomeUnavailable);
    was_side_panel_available_for_page_ = can_show_side_panel_for_page;
  }

  // Once the anchor element is visible, maybe show promo for the toolbar
  // button.
  if (toolbar_button_ && can_show_side_panel_for_page &&
      tab_contents_helper->returned_to_previous_srp_count() > 0) {
    browser_view_->MaybeShowFeaturePromo(
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
  if (web_view_->GetVisible() && web_view_->web_contents())
    web_view_->web_contents()->Focus();
}

void SideSearchBrowserController::RecordSidePanelOpenedMetrics() {
  auto* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  auto* helper = SideSearchTabContentsHelper::FromWebContents(active_contents);
  if (helper)
    helper->MaybeRecordDurationSidePanelAvailableToFirstOpen();
}
