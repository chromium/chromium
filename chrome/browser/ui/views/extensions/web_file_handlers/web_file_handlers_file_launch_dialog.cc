// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/web_file_handlers/web_file_handlers_file_launch_dialog.h"

#include <string>

#include "base/files/safe_base_name.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "extensions/common/constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebFileHandlersFileLaunchDialogCheckbox);

namespace extensions::file_handlers {

namespace {

using CallbackType = base::OnceCallback<void(bool, bool)>;

// Default checkbox value, specifically added for testing purposes.
bool g_default_remember_selection = false;

class WebFileHandlersFileLaunchDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit WebFileHandlersFileLaunchDialogDelegate(
      CallbackType callback,
      ui::ElementIdentifier checkbox_identifier)
      : callback_(std::move(callback)),
        checkbox_identifier_(checkbox_identifier) {}

  ~WebFileHandlersFileLaunchDialogDelegate() override = default;

  // Determine whether to open the file and maybe remember the decision.
  void OnDialogAccepted() { Run(/*should_open=*/true); }

  void OnDialogClosed() { Run(/*should_open=*/false); }

  // If escape is pressed, neither open the file nor remember the decision.
  void SetActionCloseCallback() {
    std::move(callback_).Run(/*should_open=*/false, /*should_remember=*/false);
  }

 private:
  CallbackType callback_;

  void Run(bool should_open) {
    auto* checkbox =
        this->dialog_model()->GetCheckboxByUniqueId(checkbox_identifier_);
    bool should_remember = checkbox->is_checked();
    std::move(callback_).Run(should_open, should_remember);
  }

  // The element identifier is declared once by this parent and reused here.
  ui::ElementIdentifier checkbox_identifier_;
};

}  // namespace

base::AutoReset<bool> SetDefaultRememberSelectionForTesting(  // IN-TEST
    bool remember_selection) {
  g_default_remember_selection = remember_selection;
  return base::AutoReset<bool>(&g_default_remember_selection,
                               remember_selection);
}

void ShowWebFileHandlersFileLaunchDialog(
    const std::vector<base::SafeBaseName>& base_names,
    const std::vector<std::u16string>& file_types,
    CallbackType callback) {
  auto checkbox_id =
      ui::ElementIdentifier(kWebFileHandlersFileLaunchDialogCheckbox);

  auto bubble_delegate_unique =
      std::make_unique<WebFileHandlersFileLaunchDialogDelegate>(
          std::move(callback), checkbox_id);
  WebFileHandlersFileLaunchDialogDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  std::u16string title = base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_WFH_PERMISSION_HANDLER_FILES),
      "FILE_COUNT", static_cast<int>(base_names.size()), "FILE1",
      base_names[0].path().value());

  // Prepare every file extension for display in the checkbox.
  const auto file_types_for_display = base::JoinString(
      file_types,
      l10n_util::GetStringUTF16(IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR));
  ui::DialogModelLabel checkbox_label =
      ui::DialogModelLabel(base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_WEB_APP_FILE_HANDLING_DIALOG_STICKY_CHOICE),
          "FILE_TYPE_COUNT", static_cast<int>(file_types.size()), "FILE_TYPES",
          file_types_for_display));

  // Prepare checkbox for testing.
  ui::DialogModelCheckbox::Params checkbox_params;
  checkbox_params.SetIsChecked(g_default_remember_selection);
  checkbox_params.SetVisible(true);

  // TODO(crbug.com/40269541): Add extension name and icon. Show files. Design:
  // https://docs.google.com/document/d/1h7ZjryB2zYEjUG9DqPLzAM1iSUXr8ZadUUY02ycExAQ
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetInternalName("WebFileHandlersFileLaunchDialogView")
          .SetTitle(title)
          .AddCheckbox(checkbox_id, checkbox_label, checkbox_params)
          .AddOkButton(
              base::BindOnce(
                  &WebFileHandlersFileLaunchDialogDelegate::OnDialogAccepted,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_WEB_APP_FILE_HANDLING_POSITIVE_BUTTON)))
          .AddCancelButton(
              base::BindOnce(
                  &WebFileHandlersFileLaunchDialogDelegate::OnDialogClosed,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_WEB_APP_FILE_HANDLING_NEGATIVE_BUTTON)))
          .SetCloseActionCallback(base::BindOnce(
              &WebFileHandlersFileLaunchDialogDelegate::SetActionCloseCallback,
              base::Unretained(bubble_delegate)))
          .Build();

  std::unique_ptr<views::BubbleDialogModelHost> dialog =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kWindow);
  dialog->SetOwnedByWidget(true);
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      std::move(dialog), /*context=*/nullptr, /*parent=*/nullptr);
  modal_dialog->Show();
}

}  // namespace extensions::file_handlers
