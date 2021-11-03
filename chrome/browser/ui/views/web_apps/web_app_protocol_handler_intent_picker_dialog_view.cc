// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/common/custom_handlers/protocol_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

bool g_default_remember_selection = false;

// TODO(https://crbug.com/1248757): Reconcile the code here, and the
// code in the PWA install dialog, and URL Handler picker.
// Returns a label containing the app name.
std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name) {
  auto name_label = std::make_unique<views::Label>(
      name, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetElideBehavior(gfx::ELIDE_TAIL);
  return name_label;
}

std::unique_ptr<views::Label> CreateOriginLabel(const url::Origin& origin) {
  auto origin_label = std::make_unique<views::Label>(
      FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Elide from head to prevent origin spoofing.
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, so explicitly disable multiline.
  origin_label->SetMultiLine(false);

  return origin_label;
}

}  // namespace

// static
void WebAppProtocolHandlerIntentPickerView::Show(
    const GURL& url,
    Profile* profile,
    const web_app::AppId& app_id,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    chrome::WebAppProtocolHandlerAcceptanceCallback close_callback) {
  std::unique_ptr<WebAppProtocolHandlerIntentPickerView> view =
      std::make_unique<WebAppProtocolHandlerIntentPickerView>(
          url, profile, app_id, std::move(profile_keep_alive),
          std::move(keep_alive), std::move(close_callback));
  views::DialogDelegate::CreateDialogWidget(std::move(view),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();
}

void WebAppProtocolHandlerIntentPickerView::
    SetDefaultRememberSelectionForTesting(bool remember_selection) {
  g_default_remember_selection = remember_selection;
}

WebAppProtocolHandlerIntentPickerView::WebAppProtocolHandlerIntentPickerView(
    const GURL& url,
    Profile* profile,
    const web_app::AppId& app_id,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    chrome::WebAppProtocolHandlerAcceptanceCallback close_callback)
    : url_(std::move(url)),
      profile_(profile),
      app_id_(std::move(app_id)),
      // Pass the ScopedProfileKeepAlive into here to ensure the profile is
      // available until the dialog is closed.
      profile_keep_alive_(std::move(profile_keep_alive)),
      // Pass the ScopedKeepAlive into here ensures the process is alive until
      // the dialog is closed, and initiates the shutdown at closure if there
      // is nothing else keeping the browser alive.
      keep_alive_(std::move(keep_alive)),
      close_callback_(std::move(close_callback)) {
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  SetModalType(ui::MODAL_TYPE_NONE);
  std::u16string title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  SetTitle(title);
  SetShowCloseButton(true);
  set_draggable(true);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_WEB_APP_PERMISSION_DONT_ALLOW));

  SetAcceptCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnAccepted,
                     base::Unretained(this)));

  SetCancelCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnCanceled,
                     base::Unretained(this)));

  SetCloseCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnClosed,
                     base::Unretained(this)));
  InitChildViews();
}

WebAppProtocolHandlerIntentPickerView::
    ~WebAppProtocolHandlerIntentPickerView() = default;

gfx::Size WebAppProtocolHandlerIntentPickerView::CalculatePreferredSize()
    const {
  const int preferred_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void WebAppProtocolHandlerIntentPickerView::OnAccepted() {
  RunCloseCallback(
      /*allowed=*/true,
      /*remember_user_choice=*/remember_selection_checkbox_->GetChecked());
}

void WebAppProtocolHandlerIntentPickerView::OnCanceled() {
  RunCloseCallback(
      /*allowed=*/false,
      /*remember_user_choice=*/remember_selection_checkbox_->GetChecked());
}

void WebAppProtocolHandlerIntentPickerView::OnClosed() {
  switch (GetWidget()->closed_reason()) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      OnAccepted();
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      OnCanceled();
      break;
    default:
      RunCloseCallback(/*allowed=*/false, /*remember_user_choice=*/false);
      break;
  }
}

void WebAppProtocolHandlerIntentPickerView::InitChildViews() {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  gfx::Insets margin_insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl);
  set_margins(margin_insets);

  const int vertical_single_distance = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_single_distance));

  // Add "Allow app to open" label.
  auto open_app_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_PROTOCOL_HANDLER_INTENT_PICKER_QUESTION,
          content::ProtocolHandler::GetProtocolDisplayName(url_.scheme())),
      views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  open_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  open_app_label->SetMultiLine(true);
  AddChildView(std::move(open_app_label));

  // Add the app info, which will look like:
  // +-------------------------------------------------------------------+
  // |      |    app short name                                          |
  // | icon |                                                            |
  // |      |    origin                                                  |
  // +-------------------------------------------------------------------+
  {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile_);
    web_app::WebAppRegistrar& registrar = provider->registrar();
    auto app_info_view = std::make_unique<views::View>();
    int icon_label_spacing = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
    app_info_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        icon_label_spacing));

    provider->icon_manager().ReadIcons(
        app_id_, IconPurpose::ANY,
        provider->registrar().GetAppDownloadedIconSizesAny(app_id_),
        base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnIconsRead,
                       weak_ptr_factory_.GetWeakPtr()));
    icon_image_view_ =
        app_info_view->AddChildView(std::make_unique<views::ImageView>());
    icon_image_view_->SetCanProcessEventsWithinSubtree(false);
    icon_image_view_->SetImageSize(
        gfx::Size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall));

    auto app_name_publisher_view = std::make_unique<views::View>();
    app_name_publisher_view->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    app_name_publisher_view->AddChildView(
        CreateNameLabel(base::UTF8ToUTF16(registrar.GetAppShortName(app_id_)))
            .release());
    app_name_publisher_view->AddChildView(
        CreateOriginLabel(
            url::Origin::Create(registrar.GetAppStartUrl(app_id_)))
            .release());
    app_info_view->AddChildView(std::move(app_name_publisher_view));

    AddChildView(std::move(app_info_view));
  }

  remember_selection_checkbox_ =
      AddChildView(std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
          IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION)));
  remember_selection_checkbox_->SetChecked(g_default_remember_selection);
}

void WebAppProtocolHandlerIntentPickerView::RunCloseCallback(
    bool allowed,
    bool remember_user_choice) {
  if (close_callback_) {
    std::move(close_callback_).Run(allowed, remember_user_choice);
  }
}

void WebAppProtocolHandlerIntentPickerView::OnIconsRead(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty() || !icon_image_view_)
    return;

  gfx::Size image_size{web_app::kWebAppIconSmall, web_app::kWebAppIconSmall};
  auto imageSkia = gfx::ImageSkia(std::make_unique<WebAppInfoImageSource>(
                                      web_app::kWebAppIconSmall, icon_bitmaps),
                                  image_size);
  icon_image_view_->SetImage(imageSkia);
}

BEGIN_METADATA(WebAppProtocolHandlerIntentPickerView, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowWebAppProtocolHandlerIntentPicker(
    const GURL& url,
    Profile* profile,
    const web_app::AppId& app_id,
    WebAppProtocolHandlerAcceptanceCallback close_callback) {
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);
  // Sometimes it is too early for registrar to be populated at this time. We
  // need to wait for it to get the web application info.
  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(WebAppProtocolHandlerIntentPickerView::Show, url, profile,
                     app_id, std::move(profile_keep_alive),
                     std::move(keep_alive), std::move(close_callback)));
}

}  // namespace chrome
