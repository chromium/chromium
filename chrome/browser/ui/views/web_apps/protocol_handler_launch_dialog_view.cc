// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/protocol_handler_launch_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"

namespace web_app {

ProtocolHandlerLaunchDialogView::ProtocolHandlerLaunchDialogView(
    GURL url,
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback)
    : LaunchAppUserChoiceDialogView(profile, app_id, std::move(close_callback)),
      url_(std::move(url)) {
  auto* layout_provider = views::LayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));
  set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_WEB_APP_PERMISSION_NEGATIVE_BUTTON));
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
}

ProtocolHandlerLaunchDialogView::~ProtocolHandlerLaunchDialogView() = default;

std::unique_ptr<views::View>
ProtocolHandlerLaunchDialogView::CreateAboveAppInfoView() {
  // Add "Allow app to open" label.
  auto open_app_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_PROTOCOL_HANDLER_INTENT_PICKER_QUESTION,
          custom_handlers::ProtocolHandler::GetProtocolDisplayName(
              url_.scheme())),
      views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  open_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  open_app_label->SetMultiLine(true);
  return open_app_label;
}

std::unique_ptr<views::View>
ProtocolHandlerLaunchDialogView::CreateBelowAppInfoView() {
  return nullptr;
}

std::u16string ProtocolHandlerLaunchDialogView::GetRememberChoiceString() {
  return l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION);
}

BEGIN_METADATA(ProtocolHandlerLaunchDialogView)
END_METADATA

void ShowWebAppProtocolLaunchDialog(
    const GURL& url,
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback) {
  auto view = std::make_unique<web_app::ProtocolHandlerLaunchDialogView>(
      url, profile, app_id, std::move(close_callback));
  view->Init();
  views::DialogDelegate::CreateDialogWidget(std::move(view),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();
}

}  // namespace web_app
