// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "components/payments/content/ssl_validity_checker.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/payments/core/url_util.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace payments {
namespace {

std::u16string GetPaymentHandlerDialogTitle(
    content::WebContents* web_contents) {
  if (!web_contents)
    return std::u16string();

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

// Returns a Google color closest to light_mode_color or dark_mode_color based
// on whether background_color is considered dark mode, with a minimum
// contrast_ratio between the returned color and the background_color.
SkColor GetContrastingGoogleColor(SkColor light_mode_color,
                                  SkColor dark_mode_color,
                                  SkColor background_color,
                                  float contrast_ratio) {
  const SkColor preferred_color = color_utils::IsDark(background_color)
                                      ? dark_mode_color
                                      : light_mode_color;
  return color_utils::PickGoogleColor(preferred_color, background_color,
                                      contrast_ratio);
}

}  // namespace

// The close ('X') button used in the PaymentHandler header UX. See
// |PopulateSheetHeaderView|.
class PaymentHandlerCloseButton : public views::ImageButton {
  METADATA_HEADER(PaymentHandlerCloseButton, views::ImageButton)

 public:
  explicit PaymentHandlerCloseButton(
      views::Button::PressedCallback pressed_callback,
      const SkColor enabled_color,
      const SkColor disabled_color)
      : views::ImageButton(std::move(pressed_callback)) {
    ConfigureVectorImageButton(this);
    views::InstallCircleHighlightPathGenerator(this);
    constexpr int kCloseButtonSize = 16;
    SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetID(static_cast<int>(DialogViewID::CANCEL_BUTTON));
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_PAYMENTS_CLOSE));

    // This view does not set its color using the browser theme color, as this
    // may differ from the header color, which is based on the web view theme.
    views::SetImageFromVectorIconWithColor(this, vector_icons::kCloseIcon,
                                           enabled_color, disabled_color);
  }
};

BEGIN_METADATA(PaymentHandlerCloseButton)
END_METADATA

PaymentHandlerWebFlowViewController::PaymentHandlerWebFlowViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    content::WebContents* payment_request_web_contents,
    Profile* profile,
    GURL target,
    PaymentHandlerOpenWindowCallback first_navigation_complete_callback)
    : PaymentRequestSheetController(spec, state, dialog),
      log_(payment_request_web_contents),
      profile_(profile),
      target_(target),
      first_navigation_complete_callback_(
          std::move(first_navigation_complete_callback)),
      dialog_manager_delegate_(payment_request_web_contents) {}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {
  if (web_contents()) {
    auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
        web_contents());
    if (manager)
      manager->SetDelegate(nullptr);
  }
  state()->OnPaymentAppWindowClosed();
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
    progress_bar_ = header_content_separator_container()->AddChildView(
        std::make_unique<views::ProgressBar>());
    progress_bar_->SetPreferredHeight(2);
  }

  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* web_view =
      content_view->AddChildView(std::make_unique<views::WebView>(profile_));
  Observe(web_view->GetWebContents());
  PaymentHandlerNavigationThrottle::MarkPaymentHandlerWebContents(
      web_contents());
  web_contents()->SetDelegate(this);
  DCHECK_NE(log_.web_contents(), web_contents());
  content::PaymentAppProvider::GetOrCreateForWebContents(
      /*payment_request_web_contents=*/log_.web_contents())
      ->SetOpenedWindow(
          /*payment_handler_web_contents=*/web_contents());
  web_view->LoadInitialURL(target_);

  if (base::FeatureList::IsEnabled(
          features::kPaymentHandlerWindowInTaskManager)) {
    // Make the web view show up in the task manager.
    task_manager::WebContentsTags::CreateForTabContents(web_contents());
  }

  // Enable modal dialogs for web-based payment handlers.
  dialog_manager_delegate_.SetWebContents(web_contents());
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(&dialog_manager_delegate_);

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
  // The PaymentHandler header consists of the payment app icon (if available),
  // the current web contents origin, and a close button. The origin is centered
  // on the dialog, whilst the icon and close are aligned with the LHS and RHS
  // respectively.
  //
  // +-----------------------------------------+
  // | ICON |          origin          | CLOSE |
  // +-----------------------------------------+

  container->SetID(static_cast<int>(DialogViewID::PAYMENT_APP_HEADER));
  container->SetBackground(GetHeaderBackground(container));
  constexpr int kVerticalInset = 8;
  constexpr int kHeaderHorizontalInset = 16;
  container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalInset, kHeaderHorizontalInset, kVerticalInset,
                        kHeaderHorizontalInset)));

  views::TableLayout* layout =
      container->SetLayoutManager(std::make_unique<views::TableLayout>());

  // Icon column.
  const SkBitmap* icon_bitmap = state()->selected_app()->icon_bitmap();
  const bool has_icon = icon_bitmap && !icon_bitmap->drawsNothing();
  constexpr int kHeaderIconWidth = 32;
  if (has_icon) {
    layout->AddColumn(views::LayoutAlignment::kStart,
                      views::LayoutAlignment::kCenter,
                      views::TableLayout::kFixedSize,
                      views::TableLayout::ColumnSize::kFixed, kHeaderIconWidth,
                      /*min_width=*/0);
  } else {
    layout->AddPaddingColumn(views::TableLayout::kFixedSize, kHeaderIconWidth);
  }

  // Origin column.
  layout->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
      /*horizontal_resize=*/1.0, views::TableLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0,
      /*min_width=*/0);

  // Close button column.
  layout->AddColumn(
      views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      /*fixed_width=*/32,
      /*min_width=*/0);

  layout->AddRows(1, views::TableLayout::kFixedSize);

  // Add the icon to the header. As we support non-square icons, resize it to
  // fit the target header height.
  //
  // We should set image size in density independent pixels here, since
  // views::ImageView objects are rastered at the device scale factor.
  if (has_icon) {
    views::ImageView* app_icon_view = container->AddChildView(CreateAppIconView(
        /*icon_resource_id=*/0, icon_bitmap,
        /*tooltip_text=*/l10n_util::GetStringUTF16(IDS_PAYMENT_HANDLER_ICON)));
    app_icon_view->SetID(
        static_cast<int>(DialogViewID::PAYMENT_APP_HEADER_ICON));
    // TODO(crbug.com/40259861): If the downloaded app icon was a vector image,
    // see if we can store and rasterize it here instead of at download time.
    float adjusted_width =
        icon_bitmap->width() *
        (IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
         base::checked_cast<float>(icon_bitmap->height()));
    app_icon_view->SetImageSize(gfx::Size(
        adjusted_width,
        IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
  }

  // Add the origin label.
  const url::Origin origin =
      web_contents()
          ? web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()
          : url::Origin::Create(target_);
  auto* origin_label = container->AddChildView(std::make_unique<views::Label>(
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);
  origin_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
  origin_label->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  // Turn off autoreadability because the computed foreground color takes
  // contrast into account.
  SkColor background_color = container->background()->get_color();
  // Get the closest label color to kColorPrimaryForeground, with a minimum
  // readable contrast ratio.
  SkColor foreground = GetContrastingGoogleColor(
      gfx::kGoogleGrey900, gfx::kGoogleGrey200, background_color,
      color_utils::kMinimumReadableContrastRatio);
  origin_label->SetAutoColorReadabilityEnabled(false);
  origin_label->SetEnabledColor(foreground);
  origin_label->SetBackgroundColor(background_color);

  if (progress_bar_) {
    // Set the progress bar colors based on the header background color. The
    // progress bar's background color serves as a separator between the header
    // and content.

    // Get the closest progress bar color to kColorProgressBar, with a minimum
    // contrast ratio used for glyphs.
    const SkColor progress_bar_color = GetContrastingGoogleColor(
        gfx::kGoogleBlue600, gfx::kGoogleBlue300, background_color,
        color_utils::kMinimumVisibleContrastRatio);

    // Get the closest separator color to kColorSeparator, with a minimum
    // contrast ratio of the default light separator contrast on white, which is
    // less than color_utils::kMinimumVisibleContrastRatio.
    const SkColor separator_color = GetContrastingGoogleColor(
        gfx::kGoogleGrey300, gfx::kGoogleGrey800, background_color,
        color_utils::GetContrastRatio(gfx::kGoogleGrey300, SK_ColorWHITE));

    progress_bar_->SetForegroundColor(progress_bar_color);
    progress_bar_->SetBackgroundColor(separator_color);
  }

  // Finally, add the close button.
  // Get the closest icon color to kColorIcon, with a minimum contrast ratio
  // used for glyphs.
  const SkColor close_icon_color = GetContrastingGoogleColor(
      gfx::kGoogleGrey500, gfx::kGoogleGrey700, background_color,
      color_utils::kMinimumVisibleContrastRatio);
  const SkColor close_icon_disabled_color = color_utils::AlphaBlend(
      close_icon_color, background_color, gfx::kDisabledControlAlpha);
  container->AddChildView(std::make_unique<PaymentHandlerCloseButton>(
      base::BindRepeating(&PaymentRequestSheetController::CloseButtonPressed,
                          GetWeakPtr()),
      close_icon_color, close_icon_disabled_color));
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
    UpdateHeaderView();
  }
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
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
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

void PaymentHandlerWebFlowViewController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_active())
    return;

  // Ignore non-primary main frame or same page navigations which aren't
  // relevant to below.
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame())
    return;

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
        .Run(true, web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
             web_contents()->GetPrimaryMainFrame()->GetRoutingID());
  }

  UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::LoadProgressChanged(double progress) {
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
  UpdateHeaderView();

  std::u16string title = GetPaymentHandlerDialogTitle(web_contents());
  if (!title.empty())
    dialog()->OnPaymentHandlerTitleSet();
}

void PaymentHandlerWebFlowViewController::AbortPayment() {
  if (web_contents())
    web_contents()->Close();

  state()->OnPaymentResponseError(errors::kPaymentHandlerInsecureNavigation);
}

std::unique_ptr<views::Background>
PaymentHandlerWebFlowViewController::GetHeaderBackground(
    views::View* header_view) {
  DCHECK(header_view);
  auto default_header_background =
      views::CreateThemedSolidBackground(ui::kColorDialogBackground);
  if (web_contents() && header_view->GetWidget()) {
    // Make sure the color is actually set before using it.
    default_header_background->OnViewThemeChanged(header_view);
    return views::CreateSolidBackground(color_utils::GetResultingPaintColor(
        web_contents()->GetThemeColor().value_or(SK_ColorTRANSPARENT),
        default_header_background->get_color()));
  }
  return default_header_background;
}

}  // namespace payments
