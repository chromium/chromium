// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_LIVE_TRANSLATE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_LIVE_TRANSLATE_COMBOBOX_MODEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ui/base/models/combobox_model.h"

class Profile;

// The model for the combobox to select a language. This is used by the global
// media control to select a Live Translate target language.
class LiveTranslateComboboxModel : public ui::ComboboxModel {
 public:
  explicit LiveTranslateComboboxModel(Profile* profile);
  LiveTranslateComboboxModel(const LiveTranslateComboboxModel&) = delete;
  LiveTranslateComboboxModel& operator=(const LiveTranslateComboboxModel&) =
      delete;

  ~LiveTranslateComboboxModel() override;

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

  void UpdateTargetLanguageIndex(int index);

 private:
  std::vector<translate::TranslateLanguageInfo> languages_;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_LIVE_TRANSLATE_COMBOBOX_MODEL_H_
