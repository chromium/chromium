// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/payments/core/url_util.h"
#include "components/security_state/core/security_state.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {
namespace {

base::string16 GetPaymentHandlerDialogTitle(
    content::WebContents* web_contents) {
  if (!web_contents)
    return base::string16();

  const base::string16 title = web_contents->GetTitle();
  const base::string16 https_prefix =
      base::ASCIIToUTF16(url::kHttpsScheme) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);
  return base::StartsWith(title, https_prefix, base::CompareCase::SENSITIVE)
             ? base::string16()
             : title;
}

}  // namespace

class ReadOnlyOriginView : public views::View {
 public:
  ReadOnlyOriginView(const base::string16& page_title,
                     const GURL& origin,
                     const SkBitmap* icon_bitmap,
                     Profile* profile,
                     security_state::SecurityLevel security_level,
                     SkColor background_color,
                     views::ButtonListener* site_settings_listener) {
    auto title_origin_container = std::make_unique<views::View>();
    SkColor foreground = color_utils::GetColorWithMaxContrast(background_color);
    views::GridLayout* title_origin_layout =
        title_origin_container->SetLayoutManager(
            std::make_unique<views::GridLayout>());

    views::ColumnSet* columns = title_origin_layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1.0,
                       views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

    bool title_is_valid = !page_title.empty();
    if (title_is_valid) {
      title_origin_layout->StartRow(views::GridLayout::kFixedSize, 0);
      auto* title_label =
          title_origin_layout->AddView(std::make_unique<views::Label>(
              page_title, views::style::CONTEXT_DIALOG_TITLE));
      title_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
      title_label->SetFocusBehavior(
          views::View::FocusBehavior::ACCESSIBLE_ONLY);
      // Turn off autoreadability because the computed |foreground| color takes
      // contrast into account.
      title_label->SetAutoColorReadabilityEnabled(false);
      title_label->SetEnabledColor(foreground);
    }

    auto origin_container = std::make_unique<views::View>();
    views::GridLayout* origin_layout = origin_container->SetLayoutManager(
        std::make_unique<views::GridLayout>());

    columns = origin_layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                       1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    origin_layout->StartRow(views::GridLayout::kFixedSize, 0);
    if (PaymentsExperimentalFeatures::IsEnabled(
            features::kPaymentHandlerSecurityIcon)) {
      auto security_icon = std::make_unique<views::ImageView>();
      const ui::ThemeProvider& theme_provider =
          ThemeService::GetThemeProviderForProfile(profile);
      security_icon->SetImage(gfx::CreateVectorIcon(
          location_bar_model::GetSecurityVectorIcon(security_level), 16,
          GetOmniboxSecurityChipColor(&theme_provider, security_level)));
      security_icon->SetID(static_cast<int>(DialogViewID::SECURITY_ICON_VIEW));
      origin_layout->AddView(std::move(security_icon));
    }
    auto* origin_label = origin_layout->AddView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(origin.host())));
    origin_label->SetElideBehavior(gfx::ELIDE_HEAD);
    if (!title_is_valid) {
      // Set the origin as title when the page title is invalid.
      origin_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));

      // Pad to keep header as the same height as when the page title is valid.
      constexpr int kVerticalPadding = 10;
      origin_label->SetBorder(
          views::CreateEmptyBorder(kVerticalPadding, 0, kVerticalPadding, 0));
    }
    // Turn off autoreadability because the computed |foreground| color takes
    // contrast into account.
    origin_label->SetAutoColorReadabilityEnabled(false);
    origin_label->SetEnabledColor(foreground);
    origin_label->SetBackgroundColor(background_color);
    title_origin_layout->StartRow(views::GridLayout::kFixedSize, 0);
    title_origin_layout->AddView(std::move(origin_container));

    views::GridLayout* top_level_layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* top_level_columns = top_level_layout->AddColumnSet(0);
    top_level_columns->AddColumn(
        views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    const bool has_icon = icon_bitmap && !icon_bitmap->drawsNothing();
    float adjusted_width = base::checked_cast<float>(has_icon ? icon_bitmap->width() : 0);
    if (has_icon) {
      adjusted_width =
          adjusted_width *
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
          icon_bitmap->height();
      // A column for the app icon.
      top_level_columns->AddColumn(
          views::GridLayout::LEADING, views::GridLayout::FILL,
          views::GridLayout::kFixedSize, views::GridLayout::ColumnSize::kFixed,
          adjusted_width,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight);
      top_level_columns->AddPaddingColumn(views::GridLayout::kFixedSize, 8);
    }

    top_level_layout->StartRow(views::GridLayout::kFixedSize, 0);
    top_level_layout->AddView(std::move(title_origin_container));
    if (has_icon) {
      views::ImageView* app_icon_view = top_level_layout->AddView(
          CreateAppIconView(/*icon_id=*/0, icon_bitmap,
                            /*label=*/page_title));
      // We should set image size in density independent pixels here, since
      // views::ImageView objects are rastered at the device scale factor.
      app_icon_view->SetImageSize(gfx::Size(
          adjusted_width,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
    }
  }
  ReadOnlyOriginView(const ReadOnlyOriginView&) = delete;
  ReadOnlyOriginView& operator=(const ReadOnlyOriginView&) = delete;
  ~ReadOnlyOriginView() override = default;
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
      // Borrow the browser's WebContentModalDialogHost to display modal dialogs
      // triggered by the payment handler's web view (e.g. WebAuthn dialogs).
      // The browser's WebContentModalDialogHost is valid throughout the
      // lifetime of this controller because the payment sheet itself is a modal
      // dialog.
      dialog_manager_delegate_(
          static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(
              chrome::FindBrowserWithWebContents(payment_request_web_contents))
              ->GetWebContentsModalDialogHost()) {}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {
  state()->OnPaymentAppWindowClosed();
}

base::string16 PaymentHandlerWebFlowViewController::GetSheetTitle() {
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
    progress_bar_->SetForegroundColor(gfx::kGoogleBlue500);
    progress_bar_->SetBackgroundColor(SK_ColorTRANSPARENT);
    progress_bar_->SetVisible(false);
    separator_ = header_content_separator_container()->AddChildView(
        std::make_unique<views::Separator>());
  }

  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* web_view =
      content_view->AddChildView(std::make_unique<views::WebView>(profile_));
  Observe(web_view->GetWebContents());
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
  web_view->SetPreferredSize(
      gfx::Size(0, dialog()->GetActualPaymentHandlerDialogHeight() - 75));
}

bool PaymentHandlerWebFlowViewController::ShouldShowSecondaryButton() {
  return false;
}

std::unique_ptr<views::View>
PaymentHandlerWebFlowViewController::CreateHeaderContentView(
    views::View* header_view) {
  const GURL origin = web_contents()
                          ? web_contents()->GetVisibleURL().GetOrigin()
                          : target_.GetOrigin();
  std::unique_ptr<views::Background> background =
      GetHeaderBackground(header_view);
  return std::make_unique<ReadOnlyOriginView>(
      GetPaymentHandlerDialogTitle(web_contents()), origin,
      state()->selected_app()->icon_bitmap(), profile_,
      web_contents() ? SslValidityChecker::GetSecurityLevel(web_contents())
                     : security_state::NONE,
      background->get_color(), this);
}

std::unique_ptr<views::Background>
PaymentHandlerWebFlowViewController::GetHeaderBackground(
    views::View* header_view) {
  auto default_header_background =
      PaymentRequestSheetController::GetHeaderBackground(header_view);
  if (web_contents()) {
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

void PaymentHandlerWebFlowViewController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsSameDocument())
    UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // Open new foreground tab or popup triggered by user activation in payment
  // handler window in browser.
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (browser && user_gesture &&
      (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
       disposition == WindowOpenDisposition::NEW_POPUP)) {
    chrome::AddWebContents(browser, source, std::move(new_contents), target_url,
                           disposition, initial_rect);
  }
}

void PaymentHandlerWebFlowViewController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_active())
    return;

  if (navigation_handle->IsSameDocument())
    return;

  // The navigation must be committed because WebContents::GetLastCommittedURL()
  // is assumed to be the URL loaded in the payment handler window.
  DCHECK(navigation_handle->HasCommitted());

  if (!SslValidityChecker::IsValidPageInPaymentHandlerWindow(
          navigation_handle->GetWebContents())) {
    AbortPayment();
    return;
  }

  if (first_navigation_complete_callback_) {
    std::move(first_navigation_complete_callback_)
        .Run(true, web_contents()->GetMainFrame()->GetProcess()->GetID(),
             web_contents()->GetMainFrame()->GetRoutingID());
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
}

void PaymentHandlerWebFlowViewController::AbortPayment() {
  if (web_contents())
    web_contents()->Close();

  state()->OnPaymentResponseError(errors::kPaymentHandlerInsecureNavigation);
}

}  // namespace payments
