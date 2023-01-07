// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace chrome {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWindowNameFieldId);

void SetBrowserTitleFromTextfield(Browser* browser,
                                  ui::DialogModel* dialog_model) {
  std::string text = base::UTF16ToUTF8(
      dialog_model->GetTextfieldByUniqueId(kWindowNameFieldId)->text());
  if (text.empty())
    base::RecordAction(base::UserMetricsAction("WindowNaming_Cleared"));
  else
    base::RecordAction(base::UserMetricsAction("WindowNaming_Set"));
  browser->SetWindowUserTitle(text);
}

std::unique_ptr<ui::DialogModel> CreateWindowNamePromptDialogModel(
    Browser* browser) {
  ui::DialogModel::Builder dialog_builder;
  return dialog_builder.SetInternalName("WindowNamePrompt")
      .SetTitle(l10n_util::GetStringUTF16(IDS_NAME_WINDOW_PROMPT_TITLE))
      .AddOkButton(base::BindOnce(&SetBrowserTitleFromTextfield, browser,
                                  dialog_builder.model()))
      .AddCancelButton(base::DoNothing())
      .AddTextfield(
          kWindowNameFieldId,
          // Deliberately use no label - the dialog contains only this
          // textfield, and its title serves as a label for the textfield.
          {}, base::UTF8ToUTF16(browser->user_title()),
          // Despite what the above comment says, the textfield still needs an
          // accessible name - otherwise a screenreader user with their focus on
          // the field will have no context for what the field means.
          ui::DialogModelTextfield::Params().SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_NAME_WINDOW_PROMPT_FIELD_LABEL)))
      .SetInitiallyFocusedField(kWindowNameFieldId)
      .Build();
}

}  // namespace

void ShowWindowNamePrompt(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("WindowNaming_DialogShown"));

  ShowBrowserModal(browser, CreateWindowNamePromptDialogModel(browser));
}

std::unique_ptr<ui::DialogModel> CreateWindowNamePromptDialogModelForTesting(
    Browser* browser) {
  return CreateWindowNamePromptDialogModel(browser);
}

}  // namespace chrome
