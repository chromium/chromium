// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/target_language_combobox_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"

TargetLanguageComboboxModel::TargetLanguageComboboxModel(
    int default_index,
    TranslateBubbleModel* model)
    : default_index_(default_index < 0 ? 0 : default_index), model_(model) {
  // view::Combobox can't treate an negative index, but |default_index| can be
  // negative when, for example, the page's language can't be detected.
}

TargetLanguageComboboxModel::~TargetLanguageComboboxModel() {}

int TargetLanguageComboboxModel::GetItemCount() const {
  return model_->GetNumberOfLanguages();
}

base::string16 TargetLanguageComboboxModel::GetItemAt(int index) {
  return model_->GetLanguageNameAt(index);
}

int TargetLanguageComboboxModel::GetDefaultIndex() const {
  return default_index_;
}
