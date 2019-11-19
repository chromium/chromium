// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {
namespace {

base::string16 GetPaymentHandlerDialogTitle(
    content::WebContents* web_contents,
    const base::string16& https_prefix) {
  return web_contents == nullptr ||
                 base::StartsWith(web_contents->GetTitle(), https_prefix,
                                  base::CompareCase::SENSITIVE)
             ? base::string16()
             : web_contents->GetTitle();
}

}  // namespace

class ReadOnlyOriginView : public views::View {
 public:
  ReadOnlyOriginView(const base::string16& page_title,
                     const GURL& origin,
                     gfx::ImageSkia icon_image_skia,
                     SkColor background_color,
                     views::ButtonListener* site_settings_listener) {
    std::unique_ptr<views::View> title_origin_container =
        std::make_unique<views::View>();
    SkColor foreground = color_utils::GetColorWithMaxContrast(background_color);
    views::GridLayout* title_origin_layout =
        title_origin_container->SetLayoutManager(
            std::make_unique<views::GridLayout>());

    views::ColumnSet* columns = title_origin_layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1.0,
                       views::GridLayout::USE_PREF, 0, 0);

    bool title_is_valid = !page_title.empty();
    if (title_is_valid) {
      title_origin_layout->StartRow(views::GridLayout::kFixedSize, 0);
      std::unique_ptr<views::Label> title_label =
          std::make_unique<views::Label>(page_title,
                                         views::style::CONTEXT_DIALOG_TITLE);
      title_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
      title_label->SetFocusBehavior(
          views::View::FocusBehavior::ACCESSIBLE_ONLY);
      // Turn off autoreadability because the computed |foreground| color takes
      // contrast into account.
      title_label->SetAutoColorReadabilityEnabled(false);
      title_label->SetEnabledColor(foreground);
      title_origin_layout->AddView(std::move(title_label));
    }

    title_origin_layout->StartRow(views::GridLayout::kFixedSize, 0);
    auto origin_label =
        std::make_unique<views::Label>(base::UTF8ToUTF16(origin.host()));
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
    title_origin_layout->AddView(std::move(origin_label));

    views::GridLayout* top_level_layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* top_level_columns = top_level_layout->AddColumnSet(0);
    top_level_columns->AddColumn(views::GridLayout::LEADING,
                                 views::GridLayout::CENTER, 1.0,
                                 views::GridLayout::USE_PREF, 0, 0);
    const bool has_icon = icon_image_skia.width() && icon_image_skia.height();
    float adjusted_width = base::checked_cast<float>(icon_image_skia.width());
    if (has_icon) {
      adjusted_width =
          adjusted_width *
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
          icon_image_skia.height();
      // A column for the app icon.
      top_level_columns->AddColumn(
          views::GridLayout::LEADING, views::GridLayout::FILL,
          views::GridLayout::kFixedSize, views::GridLayout::FIXED,
          adjusted_width,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight);
      top_level_columns->AddPaddingColumn(views::GridLayout::kFixedSize, 8);
    }

    top_level_layout->StartRow(views::GridLayout::kFixedSize, 0);
    top_level_layout->AddView(std::move(title_origin_container));
    if (has_icon) {
      std::unique_ptr<views::ImageView> app_icon_view =
          CreateAppIconView(/*icon_id=*/0, icon_image_skia,
                            /*label=*/page_title);
      // We should set image size in density independent pixels here, since
      // views::ImageView objects are rastered at the device scale factor.
      app_icon_view->SetImageSize(gfx::Size(
          adjusted_width,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
      top_level_layout->AddView(std::move(app_icon_view));
    }
  }
  ~ReadOnlyOriginView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyOriginView);
};

PaymentHandlerWebFlowViewController::PaymentHandlerWebFlowViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    content::WebContents* payment_request_web_contents,
    Profile* profile,
    GURL target,
    PaymentHandlerOpenWindowCallback first_navigation_complete_callback)
    : PaymentRequestSheetController(spec, state, dialog),
      log_(payment_request_web_contents),
      profile_(profile),
      target_(target),
      show_progress_bar_(false),
      progress_bar_(
          std::make_unique<views::ProgressBar>(/*preferred_height=*/2)),
      separator_(std::make_unique<views::Separator>()),
      first_navigation_complete_callback_(
          std::move(first_navigation_complete_callback)),
      https_prefix_(base::UTF8ToUTF16(url::kHttpsScheme) +
                    base::UTF8ToUTF16(url::kStandardSchemeSeparator)),
      // Borrow the browser's WebContentModalDialogHost to display modal dialogs
      // triggered by the payment handler's web view (e.g. WebAuthn dialogs).
      // The browser's WebContentModalDialogHost is valid throughout the
      // lifetime of this controller because the payment sheet itself is a modal
      // dialog.
      dialog_manager_delegate_(
          static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(
              chrome::FindBrowserWithWebContents(payment_request_web_contents))
              ->GetWebContentsModalDialogHost()) {
  progress_bar_->set_owned_by_client();
  progress_bar_->SetForegroundColor(gfx::kGoogleBlue500);
  progress_bar_->SetBackgroundColor(SK_ColorTRANSPARENT);
  separator_->set_owned_by_client();
  separator_->SetColor(separator_->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_SeparatorColor));
}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {
  state()->OnPaymentAppWindowClosed();
}

base::string16 PaymentHandlerWebFlowViewController::GetSheetTitle() {
  return GetPaymentHandlerDialogTitle(web_contents(), https_prefix_);
}

void PaymentHandlerWebFlowViewController::FillContentView(
    views::View* content_view) {
  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile_);
  Observe(web_view->GetWebContents());
  web_contents()->SetDelegate(this);
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
  web_view->SetPreferredSize(gfx::Size(0, kDialogHeight - 75));
  content_view->AddChildView(web_view.release());
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
      GetPaymentHandlerDialogTitle(web_contents(), https_prefix_), origin,
      state()->selected_app()->icon_image_skia(), background->get_color(),
      this);
}

views::View*
PaymentHandlerWebFlowViewController::CreateHeaderContentSeparatorView() {
  if (show_progress_bar_)
    return progress_bar_.get();
  return separator_.get();
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
  DCHECK(source == web_contents());
  if (!SslValidityChecker::IsValidPageInPaymentHandlerWindow(source)) {
    AbortPayment();
  }
}

void PaymentHandlerWebFlowViewController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
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
    chrome::AddWebContents(browser, source, std::move(new_contents),
                           disposition, initial_rect);
  }
}

void PaymentHandlerWebFlowViewController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_active())
    return;

  if (navigation_handle->IsSameDocument())
    return;

  if (!SslValidityChecker::IsValidPageInPaymentHandlerWindow(
          navigation_handle->GetWebContents())) {
    AbortPayment();
    return;
  }

  if (first_navigation_complete_callback_) {
    std::move(first_navigation_complete_callback_)
        .Run(true, web_contents()->GetMainFrame()->GetProcess()->GetID(),
             web_contents()->GetMainFrame()->GetRoutingID());
    first_navigation_complete_callback_ = PaymentHandlerOpenWindowCallback();
  }

  UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::LoadProgressChanged(double progress) {
  progress_bar_->SetValue(progress);

  if (progress == 1.0 && show_progress_bar_) {
    show_progress_bar_ = false;
    UpdateHeaderContentSeparatorView();
    return;
  }

  if (progress < 1.0 && !show_progress_bar_) {
    show_progress_bar_ = true;
    UpdateHeaderContentSeparatorView();
    return;
  }
}

void PaymentHandlerWebFlowViewController::TitleWasSet(
    content::NavigationEntry* entry) {
  UpdateHeaderView();
}

void PaymentHandlerWebFlowViewController::DidAttachInterstitialPage() {
  AbortPayment();
}

void PaymentHandlerWebFlowViewController::AbortPayment() {
  if (web_contents())
    web_contents()->Close();

  state()->OnPaymentResponseError(errors::kPaymentHandlerInsecureNavigation);
}

}  // namespace payments
