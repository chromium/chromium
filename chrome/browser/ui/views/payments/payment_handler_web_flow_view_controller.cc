// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/payments/payment_handler_header_view_util.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "components/payments/content/ssl_validity_checker.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "components/permissions/permission_request_manager.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_constants.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace payments {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PaymentHandlerWebFlowViewController,
                                      kAppIconElementId);

namespace {

// WebContentsUserData key for retrieving PaymentHandlerWebFlowViewController
// from the payment handler's WebContents. Attached in FillContentView.
class PaymentHandlerWebFlowViewControllerWebContentsWrapper
    : public content::WebContentsUserData<
          PaymentHandlerWebFlowViewControllerWebContentsWrapper> {
 public:
  PaymentHandlerWebFlowViewController* controller() {
    return controller_.get();
  }

 private:
  friend class content::WebContentsUserData<
      PaymentHandlerWebFlowViewControllerWebContentsWrapper>;

  PaymentHandlerWebFlowViewControllerWebContentsWrapper(
      content::WebContents* web_contents,
      base::WeakPtr<PaymentHandlerWebFlowViewController> controller)
      : content::WebContentsUserData<
            PaymentHandlerWebFlowViewControllerWebContentsWrapper>(
            *web_contents),
        controller_(std::move(controller)) {}

  base::WeakPtr<PaymentHandlerWebFlowViewController> controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    PaymentHandlerWebFlowViewControllerWebContentsWrapper);

std::u16string GetPaymentHandlerDialogTitle(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::u16string();
  }

  // If a page has no explicit <title> set or if it is still loading, the title
  // may be the URL of the page. We don't wish to show that to a user as the
  // origin is also shown.
  const std::u16string title = web_contents->GetTitle();
  const std::u16string https_prefix =
      base::StrCat({url::kHttpsScheme16, url::kStandardSchemeSeparator16});
  return base::StartsWith(title, https_prefix, base::CompareCase::SENSITIVE)
             ? std::u16string()
             : title;
}

}  // namespace

// Ensures that the views::WebView created by this class has its corners
// properly rounded. This class is a ViewsObserver that waits until the view
// attaches to the widget, then manually sets its corner radii to match those of
// the dialog.
//
// TODO(crbug.com/344626785): Remove once WebViews obey parent clips.
class PaymentHandlerWebFlowViewController::RoundedCornerViewClipper
    : public views::ViewObserver {
 public:
  RoundedCornerViewClipper(views::WebView* web_view,
                           base::WeakPtr<PaymentRequestDialogView> dialog)
      : web_view_(web_view), dialog_(dialog) {
    view_observation_.Observe(web_view);
  }

  void OnViewAddedToWidget(views::View* observed_view) override {
    CHECK_EQ(web_view_, observed_view);
    // The PaymentHandler dialog has a header above the WebView, so only the
    // bottom corners should be clipped to be rounded.
    web_view_->holder()->SetNativeViewCornerRadii(gfx::RoundedCornersF(
        0.f, 0.f, dialog_->GetCornerRadius(), dialog_->GetCornerRadius()));
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    CHECK_EQ(web_view_, observed_view);
    view_observation_.Reset();
    web_view_ = nullptr;
  }

 private:
  base::ScopedObservation<views::View, ViewObserver> view_observation_{this};
  raw_ptr<views::WebView> web_view_;
  base::WeakPtr<PaymentRequestDialogView> dialog_;
};

PaymentHandlerWebFlowViewController::PaymentHandlerWebFlowViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    content::WebContents* payment_request_web_contents,
    Profile* profile,
    GURL target,
    PaymentHandlerOpenWindowCallback first_navigation_complete_callback)
    : PaymentRequestSheetController(spec, state, dialog),
      profile_(profile),
      target_(target),
      location_bar_model_(
          std::make_unique<LocationBarModelImpl>(this,
                                                 content::kMaxURLDisplayChars)),
      first_navigation_complete_callback_(
          std::move(first_navigation_complete_callback)),
      dialog_manager_delegate_(payment_request_web_contents) {}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {
  if (web_contents()) {
    auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
        web_contents());
    if (manager) {
      manager->SetDelegate(nullptr);
    }
  }
  if (state()) {
    state()->OnPaymentAppWindowClosed();
  }
}

// static
PaymentHandlerWebFlowViewController*
PaymentHandlerWebFlowViewController::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* wrapper =
      PaymentHandlerWebFlowViewControllerWebContentsWrapper::FromWebContents(
          web_contents);
  return wrapper ? wrapper->controller() : nullptr;
}

views::View* PaymentHandlerWebFlowViewController::GetPageInfoIconView() {
  if (permission_dashboard_view() &&
      permission_dashboard_view()->GetVisible() &&
      permission_dashboard_view()->GetIndicatorChip()->GetVisible()) {
    return permission_dashboard_view()->GetIndicatorChip();
  }
  return location_icon_view();
}

std::u16string PaymentHandlerWebFlowViewController::GetSheetTitle() {
  return GetPaymentHandlerDialogTitle(web_contents());
}

void PaymentHandlerWebFlowViewController::FillContentView(
    views::View* content_view) {
  // The first time this is called, also create and add the header/content
  // separator container children.  This must be done before calling
  // LoadInitialURL() below so these will be set up before that calls back to
  // LoadProgressChanged(), and it can't be done in the constructor since the
  // container doesn't exist yet.
  if (!progress_bar_) {
    // Add the progress bar to the separator container. The progress bar
    // colors will be set in PopulateSheetHeaderView.
    progress_bar_ =
        header_content_separator_container()
            ->AddChildView(std::make_unique<PaymentHandlerProgressBar>())
            ->GetWeakPtr();
  }

  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* web_view =
      content_view->AddChildView(std::make_unique<views::WebView>(profile_));
  rounded_corner_clipper_ =
      std::make_unique<RoundedCornerViewClipper>(web_view, dialog());

  // Set up the WebContents that is inside the views::WebView, which hosts the
  // payment app.
  Observe(web_view->GetWebContents());
  PaymentHandlerNavigationThrottle::MarkPaymentHandlerWebContents(
      web_contents());
  PaymentHandlerWebFlowViewControllerWebContentsWrapper::CreateForWebContents(
      web_contents(), weak_ptr_factory_.GetWeakPtr());
  web_contents()->SetDelegate(this);
  content::WebContents* parent_tab_web_contents = state()->GetWebContents();

  DCHECK_NE(parent_tab_web_contents, web_contents());
  content::PaymentAppProvider::GetOrCreateForWebContents(
      /*payment_request_web_contents=*/parent_tab_web_contents)
      ->SetOpenedWindow(
          /*payment_handler_web_contents=*/web_contents());

  if (base::FeatureList::IsEnabled(
          payments::features::kPaymentHandlerDialogUseInitiatorInUrlLoad)) {
    content::NavigationController::LoadURLParams params(target_);
    params.initiator_origin =
        url::Origin::Create(parent_tab_web_contents->GetLastCommittedURL());
    web_view->GetWebContents()->GetController().LoadURLWithParams(params);
  } else {
    web_view->LoadInitialURL(target_);
  }

  // Make the web view show up in the task manager.
  task_manager::WebContentsTags::CreateForTabContents(web_contents());

  // Install permission helpers so that permission prompts and one-time
  // permissions function within the Payment Handler window. Security state
  // is computed on demand by chrome_security_state (see
  // chrome/browser/ssl/chrome_security_state_util.h) and needs no helper.
  //
  // TODO(crbug.com/539998580): Restrict non-camera permission requests in
  // Payment Handler windows via Permissions-Policy enforcement.
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerCameraAccessUx)) {
    indicator_observation_.Reset();
    if (scoped_refptr<MediaStreamCaptureIndicator> indicator =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetMediaStreamCaptureIndicator()) {
      indicator_observation_.Observe(indicator.get());
    }
    OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  } else if (base::FeatureList::IsEnabled(
                 features::kPaymentHandlerCameraAccess)) {
    OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());
  }

  // Enable modal dialogs for web-based payment handlers.
  dialog_manager_delegate_.SetWebContents(web_contents());
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(&dialog_manager_delegate_);

  // If the web-contents for the parent tab has devtools open and the "Auto-open
  // DevTools for pop-ups" setting is enabled, trigger devtools for the Payment
  // Handler modal. This does not happen by default as Payment Handler is not a
  // regular pop-up window.
  DevToolsWindow* window = DevToolsWindow::GetInstanceForInspectedWebContents(
      parent_tab_web_contents);
  if (window && window->OpenNewWindowForPopups()) {
    DevToolsWindow::OpenDevToolsWindow(
        web_contents(), DevToolsOpenedByAction::kAutomaticForNewTarget);
  }

  // The webview must get an explicitly set height otherwise the layout doesn't
  // make it fill its container. This is likely because it has no content at the
  // time of first layout (nothing has loaded yet). Because of this, set it to
  // total_dialog_height - header_height. On the other hand, the width will be
  // properly set so it can be 0 here.
  web_view->SetPreferredSize(gfx::Size(
      0, dialog()->GetActualPaymentHandlerDialogHeight() - GetHeaderHeight()));
}

bool PaymentHandlerWebFlowViewController::ShouldShowPrimaryButton() {
  return false;
}

bool PaymentHandlerWebFlowViewController::ShouldShowSecondaryButton() {
  return false;
}

void PaymentHandlerWebFlowViewController::PopulateSheetHeaderView(
    views::View* container) {
  const SkBitmap* icon_bitmap = state()->selected_app()->icon_bitmap();
  std::u16string origin_text = url_formatter::FormatOriginForSecurityDisplay(
      web_contents()
          ? web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()
          : url::Origin::Create(target_),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  std::unique_ptr<views::BoxLayoutView> icon_view;
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerCameraAccessUx)) {
    icon_view = std::make_unique<views::BoxLayoutView>();
    icon_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    icon_view->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    icon_view->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    LocationIconView* icon =
        icon_view->AddChildView(CreatePaymentHandlerLocationIconView(
            /*icon_label_bubble_delegate=*/this,
            /*location_icon_delegate=*/this));
    location_icon_view_tracker_.SetView(icon);

    PermissionDashboardView* dashboard =
        icon_view->AddChildView(std::make_unique<PermissionDashboardView>());
    dashboard->GetIndicatorChip()->SetChipIcon(vector_icons::kVideocamIcon);
    dashboard->GetIndicatorChip()->SetTheme(
        PermissionChipTheme::kInUseActivityIndicator);
    dashboard->GetIndicatorChip()->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_CAMERA_IN_USE));
    dashboard->GetIndicatorChip()->SetCallback(base::BindRepeating(
        base::IgnoreResult(
            &PaymentHandlerWebFlowViewController::ShowPageInfoDialog),
        weak_ptr_factory_.GetWeakPtr()));
    permission_dashboard_view_tracker_.SetView(dashboard);
  }

  PaymentHandlerHeaderViews header_views = PopulatePaymentHandlerHeaderView(
      container, std::move(icon_view), icon_bitmap, origin_text,
      base::BindRepeating(&PaymentRequestSheetController::CloseButtonPressed,
                          GetWeakPtr()));
  origin_label_ = header_views.origin_label;
  close_button_ = header_views.close_button;
  SetHeaderColorsAndOriginLabelText();
}

views::View* PaymentHandlerWebFlowViewController::GetFirstFocusedView() {
  // Prevent focusing the hidden "Cancel" button (https://crbug.com/415275892).
  return close_button_ ? close_button_.get()
                       : PaymentRequestSheetController::GetFirstFocusedView();
}

bool PaymentHandlerWebFlowViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET;
  return true;
}

bool PaymentHandlerWebFlowViewController::
    DisplayDynamicBorderForHiddenContents() {
  return false;
}

bool PaymentHandlerWebFlowViewController::CanContentViewBeScrollable() {
  // The web contents is set to a constant size and will render its own
  // scrollbar if necessary.
  return false;
}

base::WeakPtr<PaymentRequestSheetController>
PaymentHandlerWebFlowViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentHandlerWebFlowViewController::VisibleSecurityStateChanged(
    content::WebContents* source) {
  DCHECK_EQ(source, web_contents());
  if (!SslValidityChecker::IsValidPageInPaymentHandlerWindow(source)) {
    AbortPayment();
  } else {
    SetHeaderColorsAndOriginLabelText();
    if (location_icon_view()) {
      location_icon_view()->Update(/*suppress_animations=*/false);
    }
  }
}

content::WebContents* PaymentHandlerWebFlowViewController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)> callback) {
  // Reject CURRENT_TAB to maintain existing behavior for internal navigations.
  // OpenURLFromTab is implemented so that external links (e.g. PageInfo "Learn
  // more") are routed to the parent tab.
  if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    return nullptr;
  }
  if (!state() || !state()->GetWebContents()) {
    return nullptr;
  }
  return state()->GetWebContents()->OpenURL(params, std::move(callback));
}

content::WebContents* PaymentHandlerWebFlowViewController::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Open new foreground tab or popup triggered by user activation in payment
  // handler window in browser.
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(profile_)->GetLastActiveBrowser();
  if (browser && user_gesture &&
      (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
       disposition == WindowOpenDisposition::NEW_POPUP)) {
    chrome::AddWebContents(browser, source, std::move(new_contents), target_url,
                           disposition, window_features);
  }
  return nullptr;
}

bool PaymentHandlerWebFlowViewController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return content_view() && content_view()->GetFocusManager() &&
         unhandled_keyboard_event_handler_.HandleKeyboardEvent(
             event, content_view()->GetFocusManager());
}

// We explicitly ignore close requests from the WebContents (e.g., via
// window.close()) to prevent merchant JS or unauthorized scripts from
// unexpectedly closing the dialog. The payment dialog lifecycle is managed
// by the browser UI and the Payment Request API.
void PaymentHandlerWebFlowViewController::CloseContents(
    content::WebContents* source) {
  // Do nothing.
}

void PaymentHandlerWebFlowViewController::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Currently we allow only video camera access (no audio), behind a
  // default-disabled flag until we have appropriate UX to inform the user.
  //
  // Note that this check assumes that content::MediaStreamRequest will not add
  // new 'types' in the future, as they will not be blocked by default. That
  // would be very unlikely, and since this is flag guarded currently anyway
  // this check suffices.
  if (request.video_type !=
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
      request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE ||
      !(base::FeatureList::IsEnabled(features::kPaymentHandlerCameraAccess) ||
        base::FeatureList::IsEnabled(
            features::kPaymentHandlerCameraAccessUx))) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
        /*ui=*/nullptr);
    return;
  }
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool PaymentHandlerWebFlowViewController::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
      !(base::FeatureList::IsEnabled(features::kPaymentHandlerCameraAccess) ||
        base::FeatureList::IsEnabled(
            features::kPaymentHandlerCameraAccessUx))) {
    return false;
  }
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   /*extension=*/nullptr);
}

void PaymentHandlerWebFlowViewController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_active()) {
    return;
  }

  // Ignore non-primary main frame or same page navigations which aren't
  // relevant to below.
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Checking uncommitted navigations (e.g., Network errors) is unnecessary
  // because the new pages have no chance to be loaded, rendered nor execute js.
  // TODO(crbug.com/40177268): Only primary main frame is checked because unsafe
  // iframes are blocked by the MixContentNavigationThrottle. But this design is
  // fragile.
  if (navigation_handle->HasCommitted() &&
      !SslValidityChecker::IsValidPageInPaymentHandlerWindow(
          navigation_handle->GetWebContents())) {
    AbortPayment();
    return;
  }

  if (first_navigation_complete_callback_) {
    std::move(first_navigation_complete_callback_)
        .Run(true,
             web_contents()
                 ->GetPrimaryMainFrame()
                 ->GetProcess()
                 ->GetDeprecatedID(),
             web_contents()->GetPrimaryMainFrame()->GetRoutingID());
  }

  SetHeaderColorsAndOriginLabelText();
  if (location_icon_view()) {
    location_icon_view()->Update(/*suppress_animations=*/false);
  }
}

void PaymentHandlerWebFlowViewController::LoadProgressChanged(double progress) {
  if (!progress_bar_) {
    return;
  }

  // The progress bar reflects the load progress until it reaches 1.0, at
  // which point it's reset to 0 to just show the separator color.
  progress_bar_->SetValue(progress < 1.0 ? progress : 0);

  // The progress bar is accessibility-visible while loading, and then ignored
  // once it just serves as a separator.
  progress_bar_->GetViewAccessibility().SetIsIgnored(progress == 1.0);
  progress_bar_->GetViewAccessibility().SetIsLeaf(progress == 1.0);
}

void PaymentHandlerWebFlowViewController::TitleWasSet(
    content::NavigationEntry* entry) {
  SetHeaderColorsAndOriginLabelText();

  std::u16string title = GetPaymentHandlerDialogTitle(web_contents());
  if (!title.empty()) {
    dialog()->OnPaymentHandlerTitleSet();
  }
}

void PaymentHandlerWebFlowViewController::AbortPayment() {
  if (web_contents()) {
    web_contents()->Close();
  }

  state()->OnPaymentResponseError(
      mojom::PaymentEventResponseType::PAYMENT_HANDLER_INSECURE_NAVIGATION,
      errors::kPaymentHandlerInsecureNavigation);
}

void PaymentHandlerWebFlowViewController::SetHeaderColorsAndOriginLabelText() {
  std::optional<SkColor> theme_color;
  if (web_contents()) {
    theme_color = web_contents()->GetThemeColor();
  }

  SetHeaderColors(header_view(), origin_label_.get(), progress_bar_.get(),
                  close_button_.get(), theme_color);

  if (origin_label_) {
    origin_label_->SetText(url_formatter::FormatOriginForSecurityDisplay(
        web_contents()
            ? web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()
            : url::Origin::Create(target_),
        url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }
}

void PaymentHandlerWebFlowViewController::DidChangeThemeColor() {
  if (base::FeatureList::IsEnabled(
          payments::features::kPaymentHandlerHtmlHeadThemeColor)) {
    SetHeaderColorsAndOriginLabelText();
    if (web_contents() && web_contents()->GetThemeColor().has_value()) {
      dialog()->OnPaymentHandlerThemeColorSet();
    }
  }
}

content::WebContents*
PaymentHandlerWebFlowViewController::GetActiveWebContents() const {
  return web_contents();
}

SkColor PaymentHandlerWebFlowViewController::
    GetIconLabelBubbleSurroundingForegroundColor() const {
  // TODO(crbug.com/549701781): The payment dialog header supports custom HTML
  // head theme colors, while this icon view currently ignores them in favor of
  // default Chrome theme colors.
  return header_view()->GetColorProvider()->GetColor(kColorOmniboxText);
}

SkColor PaymentHandlerWebFlowViewController::GetIconLabelBubbleBackgroundColor()
    const {
  // TODO(crbug.com/549701781): The payment dialog header supports custom HTML
  // head theme colors, while this icon view currently ignores them in favor of
  // default Chrome theme colors.
  return header_view()->GetColorProvider()->GetColor(
      ui::kColorDialogBackground);
}

content::WebContents* PaymentHandlerWebFlowViewController::GetWebContents() {
  return web_contents();
}

bool PaymentHandlerWebFlowViewController::IsEditingOrEmpty() const {
  return false;
}

SkColor PaymentHandlerWebFlowViewController::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  ui::ColorId id = kColorOmniboxText;
  if (security_level == security_state::DANGEROUS) {
    id = kColorOmniboxSecurityChipDangerous;
  }
  return header_view()->GetColorProvider()->GetColor(id);
}

LocationIconView* PaymentHandlerWebFlowViewController::location_icon_view() {
  return views::AsViewClass<LocationIconView>(
      location_icon_view_tracker_.view());
}

PermissionDashboardView*
PaymentHandlerWebFlowViewController::permission_dashboard_view() {
  return views::AsViewClass<PermissionDashboardView>(
      permission_dashboard_view_tracker_.view());
}

void PaymentHandlerWebFlowViewController::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  if (contents != web_contents()) {
    return;
  }
  if (location_icon_view()) {
    location_icon_view()->SetVisible(!is_capturing_video);
  }
  if (permission_dashboard_view()) {
    // PermissionDashboardView initializes its chips as hidden, so both must be
    // shown.
    permission_dashboard_view()->SetVisible(is_capturing_video);
    permission_dashboard_view()->GetIndicatorChip()->SetVisible(
        is_capturing_video);
  }
}
bool PaymentHandlerWebFlowViewController::ShowPageInfoDialog() {
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    return false;
  }
  views::View* const anchor_view = GetPageInfoIconView();
  CHECK(anchor_view);

  content::WebContents* parent_tab_contents = state()->GetWebContents();
  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(anchor_view),
          dialog()->GetWidget()->GetNativeWindow(), contents,
          contents->GetLastCommittedURL())
          .AddGetBrowserCallback(base::BindRepeating(
              [](content::WebContents* parent_contents,
                 content::WebContents*) -> BrowserWindowInterface* {
                return tabs::TabInterface::GetFromContents(parent_contents)
                    ->GetBrowserWindowInterface();
              },
              parent_tab_contents))
          .AddPageInfoClosingCallback(base::BindOnce(
              &PaymentHandlerWebFlowViewController::OnPageInfoBubbleClosed,
              weak_ptr_factory_.GetWeakPtr()))
          .HideExtendedSiteInfo()
          .Build();

  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
  bubble->SetHighlightedElement(
      anchor_view == location_icon_view()
          ? kAppIconElementId
          : PermissionChipView::kIndicatorChipElementId);
  bubble->GetWidget()->Show();
  return true;
}

void PaymentHandlerWebFlowViewController::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  if (LocationIconView* icon = location_icon_view()) {
    icon->MaybeAnimateIcon(/*open=*/false);
  }
}

const LocationBarModel*
PaymentHandlerWebFlowViewController::GetLocationBarModel() const {
  return location_bar_model_.get();
}

ui::ImageModel PaymentHandlerWebFlowViewController::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) {
  return ui::ImageModel::FromVectorIcon(
      location_bar_model_->GetVectorIcon(),
      GetSecurityChipColor(location_bar_model_->GetSecurityLevel()),
      GetLayoutConstant(LayoutConstant::kLocationBarIconSize));
}

void PaymentHandlerWebFlowViewController::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (state()) {
    state()->set_user_interaction_in_web_payment_app(true);
  }
}

void PaymentHandlerWebFlowViewController::DidStopLoading() {
  dialog()->HideLoadingView();
}

}  // namespace payments
