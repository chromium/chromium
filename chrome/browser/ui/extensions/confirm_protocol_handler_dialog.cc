// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_navigation_throttle.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/ui_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/origin.h"

using HandlerPermissionGrantedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionGrantedCallback;
using HandlerPermissionDeniedCallback = custom_handlers::
    ProtocolHandlerNavigationThrottle::HandlerPermissionDeniedCallback;

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogHandlerRedirection);
DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogRememberMeCheckbox);

namespace {

std::u16string GetMessageTextForOrigin(
    content::WebContents* web_contents,
    const std::optional<url::Origin>& origin,
    const custom_handlers::ProtocolHandler& handler) {
  CHECK(web_contents);
  // Only handlers with extension id can be unconfirmed for now.
  CHECK(handler.extension_id());
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents->GetBrowserContext());
  CHECK(registry);
  const Extension* extension = registry->GetExtensionById(
      *handler.extension_id(), ExtensionRegistry::ENABLED);
  CHECK(extension);
  if (!origin || origin->opaque()) {
    return l10n_util::GetStringFUTF16(
        IDS_CONFIRM_PROTOCOL_HANDLER_MESSAGE,
        extensions::ui_util::GetFixupExtensionNameForUIDisplay(
            extension->name()),
        handler.GetProtocolDisplayName(),
        base::UTF8ToUTF16(handler.url().host()));
  }
  return l10n_util::GetStringFUTF16(
      IDS_CONFIRM_PROTOCOL_HANDLER_MESSAGE_WITH_INITIATING_ORIGIN,
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(extension->name()),
      base::UTF8ToUTF16(handler.protocol()),
      url_formatter::FormatOriginForSecurityDisplay(*origin),
      base::UTF8ToUTF16(handler.url().host()));
}

}  // namespace

class ConfirmProtocolHandlerDialogDelegate : public ui::DialogModelDelegate {
 public:
  ConfirmProtocolHandlerDialogDelegate(
      HandlerPermissionGrantedCallback granted_callback,
      HandlerPermissionDeniedCallback denied_callback)
      : granted_callback_(std::move(granted_callback)),
        denied_callback_(std::move(denied_callback)) {}

  ~ConfirmProtocolHandlerDialogDelegate() override = default;

  void OnDialogAccepted() {
    bool remember = dialog_model()
                        ->GetCheckboxByUniqueId(
                            kConfirmProtocolHandlerDialogRememberMeCheckbox)
                        ->is_checked();

    std::move(granted_callback_).Run(remember);
  }

  void OnDialogCancelled() { std::move(denied_callback_).Run(); }

 private:
  custom_handlers::ProtocolHandlerNavigationThrottle::
      HandlerPermissionGrantedCallback granted_callback_;
  custom_handlers::ProtocolHandlerNavigationThrottle::
      HandlerPermissionDeniedCallback denied_callback_;
};

void ShowConfirmProtocolHandlerDialog(
    content::WebContents* web_contents,
    const custom_handlers::ProtocolHandler& handler,
    const std::optional<url::Origin>& initiating_origin,
    base::OnceCallback<void(bool)> granted_callback,
    base::OnceCallback<void()> denied_callback) {
  CHECK(!handler.is_confirmed());
  auto dialog_delegate_unique =
      std::make_unique<ConfirmProtocolHandlerDialogDelegate>(
          std::move(granted_callback), std::move(denied_callback));
  ConfirmProtocolHandlerDialogDelegate* dialog_delegate =
      dialog_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique))
          .SetTitle(
              l10n_util::GetStringFUTF16(IDS_CONFIRM_PROTOCOL_HANDLER_TITLE,
                                         handler.GetProtocolDisplayName()))
          .AddParagraph(ui::DialogModelLabel(GetMessageTextForOrigin(
                            web_contents, initiating_origin, handler)),
                        /*header=*/std::u16string(),
                        /*id=*/kConfirmProtocolHandlerDialogHandlerRedirection)
          .AddOkButton(
              base::BindOnce(
                  &ConfirmProtocolHandlerDialogDelegate::OnDialogAccepted,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT))
                  .SetId(views::DialogClientView::kOkButtonElementId))
          .AddCancelButton(
              base::BindOnce(
                  &ConfirmProtocolHandlerDialogDelegate::OnDialogCancelled,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_REGISTER_PROTOCOL_HANDLER_DENY))
                  .SetId(views::DialogClientView::kCancelButtonElementId))
          .AddCheckbox(kConfirmProtocolHandlerDialogRememberMeCheckbox,
                       ui::DialogModelLabel(l10n_util::GetStringFUTF16(
                           IDS_CONFIRM_PROTOCOL_HANDLER_REMEMBER,
                           base::UTF8ToUTF16(handler.url().host()),
                           handler.GetProtocolDisplayName())))
          .Build();

  ShowWebModalDialog(web_contents, std::move(dialog_model));
}

}  // namespace extensions
