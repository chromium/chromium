// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

class TranslateLanguageListModel;

// The model for the combobox to select a language. This is used for Translate
// user interface to select language.
class SourceLanguageComboboxModel : public ui::ComboboxModel {
 public:
  SourceLanguageComboboxModel(int default_index,
                              TranslateLanguageListModel* model);

  SourceLanguageComboboxModel(const SourceLanguageComboboxModel&) = delete;
  SourceLanguageComboboxModel& operator=(const SourceLanguageComboboxModel&) =
      delete;

  ~SourceLanguageComboboxModel() override;

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

 private:
  const int default_index_;
  raw_ptr<TranslateLanguageListModel> model_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_
