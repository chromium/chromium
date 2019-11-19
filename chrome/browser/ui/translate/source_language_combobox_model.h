// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

class TranslateBubbleModel;

// The model for the combobox to select a language. This is used for Translate
// user interface to select language.
class SourceLanguageComboboxModel : public ui::ComboboxModel {
 public:
  SourceLanguageComboboxModel(int default_index, TranslateBubbleModel* model);
  ~SourceLanguageComboboxModel() override;

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  int GetDefaultIndex() const override;

 private:
  const int default_index_;
  TranslateBubbleModel* model_;

  DISALLOW_COPY_AND_ASSIGN(SourceLanguageComboboxModel);
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_SOURCE_LANGUAGE_COMBOBOX_MODEL_H_
