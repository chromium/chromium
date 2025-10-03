// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"
#include "url/url_constants.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kProtocolHandlerPickerDialogRememberSelectionCheckboxId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogOkButtonId);

namespace {

std::u16string GetDialogTitle(
    const GURL& protocol_url,
    const web_app::ProtocolHandlerPickerDialogEntries& apps) {
  std::u16string protocol_scheme = base::UTF8ToUTF16(protocol_url.GetScheme()) +
                                   url::kStandardSchemeSeparator16;
  return apps.size() == 1
             ? l10n_util::GetStringFUTF16(
                   IDS_PROTOCOL_HANDLER_PICKER_TITLE_SINGLE_APP,
                   protocol_scheme,
                   gfx::TruncateString(apps[0].app_name, /*length=*/30,
                                       gfx::WORD_BREAK))
             : l10n_util::GetStringFUTF16(
                   IDS_PROTOCOL_HANDLER_PICKER_TITLE_MULTIPLE_APPS,
                   protocol_scheme);
}

ui::DialogModelLabel GetDialogParagraph(
    const std::optional<url::Origin>& initiator_origin) {
  return initiator_origin
             ? ui::DialogModelLabel::CreateWithReplacement(
                   IDS_PROTOCOL_HANDLER_PICKER_PARAGRAPH_WITH_ORIGIN,
                   ui::DialogModelLabel::CreatePlainText(
                       base::UTF8ToUTF16(initiator_origin->Serialize())))
             : ui::DialogModelLabel(
                   IDS_PROTOCOL_HANDLER_PICKER_PARAGRAPH_GENERIC);
}

}  // namespace

namespace web_app {

std::unique_ptr<ui::DialogModel> CreateProtocolHandlerPickerDialog(
    const GURL& protocol_url,
    const ProtocolHandlerPickerDialogEntries& apps,
    const std::optional<url::Origin>& initiator_origin,
    OnPickerClosedCallback callback) {
  return ui::DialogModel::Builder()
      .SetInternalName("ProtocolHandlerPickerDialog")
      .SetTitle(GetDialogTitle(protocol_url, apps))
      .AddParagraph(ui::DialogModelLabel(GetDialogParagraph(initiator_origin)))
      .AddCheckbox(
          kProtocolHandlerPickerDialogRememberSelectionCheckboxId,
          ui::DialogModelLabel::CreateWithReplacement(
              IDS_PROTOCOL_HANDLER_PICKER_DIALOG_ALWAYS_OPEN_IN_THIS_APP,
              ui::DialogModelLabel::CreatePlainText(
                  base::UTF8ToUTF16(protocol_url.GetScheme()) +
                  url::kStandardSchemeSeparator16)))
      .AddCancelButton(base::DoNothing())
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params()
                       .SetEnabled(true)
                       .SetId(kProtocolHandlerPickerDialogOkButtonId)
                       .SetLabel(l10n_util::GetStringUTF16(IDS_OPEN)))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .Build();
}

}  // namespace web_app
