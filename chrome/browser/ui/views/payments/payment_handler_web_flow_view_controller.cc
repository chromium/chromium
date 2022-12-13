// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
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

}  // namespace

// The header view for the current PaymentHandler UX (see also
// |PopulateSheetHeaderView| below for how the future UX is drawn). Displays a
// header consisting of:
//
//   - The payment app title (taken from the WebContents)
//   - The payment app origin
//   - The payment app icon (if present)
//
// Note that the back button on the header is drawn instead by the parent
// PaymentRequestSheetController class in the current UX.
class ReadOnlyOriginView : public views::View {
 public:
  METADATA_HEADER(ReadOnlyOriginView);
  ReadOnlyOriginView(const std::u16string& page_title,
                     const url::Origin& origin,
                     const SkBitmap* icon_bitmap,
                     Profile* profile,
                     SkColor background_color) {
    auto title_origin_container = std::make_unique<views::View>();
    SkColor foreground = color_utils::GetColorWithMaxContrast(background_color);
    title_origin_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));

    bool title_is_valid = !page_title.empty();
    if (title_is_valid) {
      auto* title_label =
          title_origin_container->AddChildView(std::make_unique<views::Label>(
              page_title, views::style::CONTEXT_DIALOG_TITLE));
      title_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
      title_label->SetFocusBehavior(
          views::View::FocusBehavior::ACCESSIBLE_ONLY);
      // Turn off autoreadability because the computed |foreground| color takes
      // contrast into account.
      title_label->SetAutoColorReadabilityEnabled(false);
      title_label->SetEnabledColor(foreground);
      title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    }

    auto* origin_label =
        title_origin_container->AddChildView(std::make_unique<views::Label>(
            url_formatter::FormatOriginForSecurityDisplay(
                origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
    origin_label->SetElideBehavior(gfx::ELIDE_HEAD);
    if (!title_is_valid) {
      // Set the origin as title when the page title is invalid.
      origin_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));

      // Pad to keep header as the same height as when the page title is valid.
      constexpr int kVerticalPadding = 10;
      origin_label->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(kVerticalPadding, 0, kVerticalPadding, 0)));
    }
    // Turn off autoreadability because the computed |foreground| color takes
    // contrast into account.
    origin_label->SetAutoColorReadabilityEnabled(false);
    origin_label->SetEnabledColor(foreground);
    origin_label->SetBackgroundColor(background_color);
    origin_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title_origin_container->AddChildView(std::move(origin_label));

    views::BoxLayout* top_level_layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>());
    const bool has_icon = icon_bitmap && !icon_bitmap->drawsNothing();
    float adjusted_width = base::checked_cast<float>(has_icon ? icon_bitmap->width() : 0);
    if (has_icon) {
      adjusted_width =
          adjusted_width *
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
          icon_bitmap->height();
    }

    // Expand the title to take the remaining width.
    top_level_layout->SetFlexForView(
        AddChildView(std::move(title_origin_container)), 1);
    if (has_icon) {
      views::ImageView* app_icon_view =
          AddChildView(CreateAppIconView(/*icon_resource_id=*/0, icon_bitmap,
                                         /*tooltip_text=*/page_title));
      app_icon_view->SetID(
          static_cast<int>(DialogViewID::PAYMENT_APP_HEADER_ICON));
      // We should set image size in density independent pixels here, since
      // views::ImageView objects are rastered at the device scale factor.
      app_icon_view->SetImageSize(gfx::Size(
          adjusted_width,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
      app_icon_view->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(0, 0, 0, 8));
    }
  }
  ReadOnlyOriginView(const ReadOnlyOriginView&) = delete;
  ReadOnlyOriginView& operator=(const ReadOnlyOriginView&) = delete;
  ~ReadOnlyOriginView() override = default;
};

BEGIN_METADATA(ReadOnlyOriginView, views::View)
END_METADATA

// The close ('X') button used in the minimal PaymentHandler header UX. See
// |PopulateSheetHeaderView|.
class PaymentHandlerCloseButton : public views::ImageButton {
 public:
  explicit PaymentHandlerCloseButton(
      views::Button::PressedCallback pressed_callback)
      : views::ImageButton(pressed_callback) {
    ConfigureVectorImageButton(this);
    views::InstallCircleHighlightPathGenerator(this);
    constexpr int kCloseButtonSize = 16;
    SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetID(static_cast<int>(DialogViewID::CANCEL_BUTTON));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_PAYMENTS_CLOSE));
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const auto* const cp = GetColorProvider();
    views::SetImageFromVectorIconWithColor(
        this, vector_icons::kCloseIcon,
        cp->GetColor(kColorPaymentsRequestBackArrowButtonIcon),
        cp->GetColor(kColorPaymentsRequestBackArrowButtonIconDisabled));
  }
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
    // Add both progress bar and separator to the container, and set the
    // separator as the initially-visible one.
    progress_bar_ = header_content_separator_container()->AddChildView(
        std::make_unique<views::ProgressBar>(/*preferred_height=*/2));
    progress_bar_->SetBackgroundColor(SK_ColorTRANSPARENT);
    progress_bar_->SetVisible(false);
    separator_ = header_content_separator_container()->AddChildView(
        std::make_unique<views::Separator>());
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

  // Enable modal dialogs for web-based payment handlers.
  dialog_manager_delegate_.SetWebContents(web_contents());
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(&dialog_manager_delegate_);

  // The webview must get an explicitly set height otherwise the layout doesn't
  // make it fill its container. This is likely because it has no content at the
  // time of first layout (nothing has loaded yet). Because of this, set it to.
  // total_dialog_height - header_height. On the other hand, the width will be
  // properly set so it can be 0 here.
  //
  // TODO(crbug.com/1385136): Correct this for the experimental minimal header
  // UX (currently it leaves a gap at the bottom of the dialog).
  web_view->SetPreferredSize(
      gfx::Size(0, dialog()->GetActualPaymentHandlerDialogHeight() - 75));
}

bool PaymentHandlerWebFlowViewController::ShouldShowPrimaryButton() {
  return false;
}

bool PaymentHandlerWebFlowViewController::ShouldShowSecondaryButton() {
  return false;
}

void PaymentHandlerWebFlowViewController::PopulateSheetHeaderView(
    views::View* container) {
  if (!base::FeatureList::IsEnabled(features::kPaymentHandlerMinimalHeaderUX)) {
    PaymentRequestSheetController::PopulateSheetHeaderView(container);
    return;
  }

  // The PaymentHandler header consists of the payment app icon (if available),
  // the current web contents origin, and a close button. The origin is centered
  // on the dialog, whilst the icon and close are aligned with the LHS and RHS
  // respectively.
  //
  // +-----------------------------------------+
  // | ICON |          origin          | CLOSE |
  // +-----------------------------------------+

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
  // TODO(crbug.com/1385136): Handle missing/empty/rectangular icons.
  DCHECK(icon_bitmap && !icon_bitmap->drawsNothing());
  layout->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      /*fixed_width=*/32,
      /*min_width=*/0);

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
  views::ImageView* app_icon_view = container->AddChildView(CreateAppIconView(
      /*icon_resource_id=*/0, icon_bitmap,
      // TODO(crbug.com/1385136): Determine correct text (used for both tooltip
      // and screen reader).
      /*tooltip_text=*/GetPaymentHandlerDialogTitle(web_contents())));
  app_icon_view->SetID(static_cast<int>(DialogViewID::PAYMENT_APP_HEADER_ICON));
  float adjusted_width =
      base::checked_cast<float>(icon_bitmap->width()) *
      (IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
       icon_bitmap->height());
  app_icon_view->SetImageSize(gfx::Size(
      adjusted_width,
      IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));

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
  // Turn off autoreadability because the computed foreground color takes
  // contrast into account.
  SkColor background_color = container->background()->get_color();
  SkColor foreground = color_utils::GetColorWithMaxContrast(background_color);
  origin_label->SetAutoColorReadabilityEnabled(false);
  origin_label->SetEnabledColor(foreground);
  origin_label->SetBackgroundColor(background_color);

  // Finally, add the close button.
  //
  // TODO(crbug.com/1385136): Close button should always close, even if the
  // browser sheet is present.
  container->AddChildView(
      std::make_unique<PaymentHandlerCloseButton>(base::BindRepeating(
          &PaymentRequestSheetController::BackButtonPressed, GetWeakPtr())));
}

std::unique_ptr<views::View>
PaymentHandlerWebFlowViewController::CreateHeaderContentView(
    views::View* header_view) {
  const url::Origin origin =
      web_contents()
          ? web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()
          : url::Origin::Create(target_);
  std::unique_ptr<views::Background> background =
      GetHeaderBackground(header_view);
  return std::make_unique<ReadOnlyOriginView>(
      GetPaymentHandlerDialogTitle(web_contents()), origin,
      state()->selected_app()->icon_bitmap(), profile_,
      background->get_color());
}

std::unique_ptr<views::Background>
PaymentHandlerWebFlowViewController::GetHeaderBackground(
    views::View* header_view) {
  DCHECK(header_view);
  auto default_header_background =
      PaymentRequestSheetController::GetHeaderBackground(header_view);
  if (web_contents() && header_view->GetWidget()) {
    // Make sure the color is actually set before using it.
    default_header_background->OnViewThemeChanged(header_view);
    return views::CreateSolidBackground(color_utils::GetResultingPaintColor(
        web_contents()->GetThemeColor().value_or(SK_ColorTRANSPARENT),
        default_header_background->get_color()));
  }
  return default_header_background;
}

bool PaymentHandlerWebFlowViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET;
  return true;
}

bool PaymentHandlerWebFlowViewController::
    DisplayDynamicBorderForHiddenContents() {
  return false;
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

void PaymentHandlerWebFlowViewController::AddNewContents(
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
}

bool PaymentHandlerWebFlowViewController::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
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
  // TODO(crbug.com/1198274): Only primary main frame is checked because unsafe
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
  progress_bar_->SetValue(progress);
  const bool show_progress = progress < 1.0;
  progress_bar_->SetVisible(show_progress);
  separator_->SetVisible(!show_progress);
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

}  // namespace payments
