// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_hover_button.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {
// This dialog follows the design that
// chrome/browser/ui/views/intent_picker_bubble_view.cc created and the
// main component sizes were also mostly copied over to share the
// same layout.
// Main components sizes
constexpr int kMaxIntentPickerWidth = 320;
constexpr int kRowHeight = 32;
constexpr int kTitlePadding = 16;

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
  std::u16string title = l10n_util::GetStringUTF16(
      IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_TITLE);
  SetTitle(title);
  SetShowCloseButton(true);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_OK_BUTTON_TEXT));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_CANCEL_BUTTON_TEXT));

  SetAcceptCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnAccepted,
                     base::Unretained(this)));

  SetCancelCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnCanceled,
                     base::Unretained(this)));

  SetCloseCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnClosed,
                     base::Unretained(this)));
  Initialize();
}

WebAppProtocolHandlerIntentPickerView::
    ~WebAppProtocolHandlerIntentPickerView() = default;

gfx::Size WebAppProtocolHandlerIntentPickerView::CalculatePreferredSize()
    const {
  return gfx::Size(kMaxIntentPickerWidth,
                   GetHeightForWidth(kMaxIntentPickerWidth));
}


void WebAppProtocolHandlerIntentPickerView::OnAccepted() {
  RunCloseCallback(/*accepted=*/true);
}

void WebAppProtocolHandlerIntentPickerView::OnCanceled() {
  RunCloseCallback(/*accepted=*/false);
}

void WebAppProtocolHandlerIntentPickerView::OnClosed() {
  if (GetWidget()->closed_reason() ==
      views::Widget::ClosedReason::kAcceptButtonClicked) {
    OnAccepted();
  } else {
    OnCanceled();
  }
}

void WebAppProtocolHandlerIntentPickerView::Initialize() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Creates a view to hold the views for each app.
  auto scrollable_view = std::make_unique<views::View>();
  scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  web_app::WebAppProvider* provider = web_app::WebAppProvider::Get(profile_);
  web_app::WebAppRegistrar& registrar = provider->registrar();
  auto app_button = std::make_unique<WebAppHoverButton>(
      views::Button::PressedCallback(), app_id_, provider,
      base::UTF8ToUTF16(registrar.GetAppShortName(app_id_)),
      registrar.GetAppStartUrl(app_id_));
  app_button->set_tag(0);
  app_button->SetTooltipAndAccessibleName();
  scrollable_view->AddChildViewAt(std::move(app_button), 0);

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetBackgroundThemeColorId(
      ui::NativeTheme::kColorId_BubbleBackground);
  scroll_view->SetContents(std::move(scrollable_view));
  // This part gives the scroll a fixed height.
  scroll_view->ClipHeightTo(kRowHeight, 2 * kRowHeight);

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed, kMaxIntentPickerWidth,
                0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetId,
                              views::GridLayout::kFixedSize, kTitlePadding);
  layout->AddView(std::move(scroll_view));
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId, 0);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kRowHeight);
}

void WebAppProtocolHandlerIntentPickerView::RunCloseCallback(bool accepted) {
  if (close_callback_) {
    std::move(close_callback_).Run(accepted);
  }
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
  auto* provider = web_app::WebAppProvider::Get(profile);
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
