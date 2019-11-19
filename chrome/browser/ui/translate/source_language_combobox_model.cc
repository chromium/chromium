// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/source_language_combobox_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/strings/grit/components_strings.h"

SourceLanguageComboboxModel::SourceLanguageComboboxModel(
    int default_index,
    TranslateBubbleModel* model)
    : default_index_(default_index < 0 ? 0 : default_index), model_(model) {
  // view::Combobox can't treate an negative index, but |default_index| can be
  // negative when, for example, the page's language can't be detected.
}

SourceLanguageComboboxModel::~SourceLanguageComboboxModel() {}

// Adds "Unknown" to top of dropdown menu.
int SourceLanguageComboboxModel::GetItemCount() const {
  return model_->GetNumberOfLanguages() + 1;
}
// Indexing increased by one due to additional option "Unknown".
base::string16 SourceLanguageComboboxModel::GetItemAt(int index) {
  if (index == 0) {
    return base::string16(
        l10n_util::GetStringUTF16(IDS_TRANSLATE_UNKNOWN_SOURCE_LANGUAGE));
  } else {
    return model_->GetLanguageNameAt(index - 1);
  }
}

int SourceLanguageComboboxModel::GetDefaultIndex() const {
  return default_index_;
}
