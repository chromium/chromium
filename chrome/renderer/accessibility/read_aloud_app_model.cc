// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_app_model.h"

#include "base/values.h"

ReadAloudAppModel::ReadAloudAppModel() = default;

ReadAloudAppModel::~ReadAloudAppModel() = default;

void ReadAloudAppModel::OnSettingsRestoredFromPrefs(
    double speech_rate,
    base::Value::List* languages_enabled_in_pref,
    base::Value::Dict* voices,
    read_anything::mojom::HighlightGranularity granularity) {
  speech_rate_ = speech_rate;
  languages_enabled_in_pref_ = languages_enabled_in_pref->Clone();
  voices_ = voices->Clone();
  highlight_granularity_ = static_cast<size_t>(granularity);
}

void ReadAloudAppModel::SetLanguageEnabled(const std::string& lang,
                                           bool enabled) {
  if (enabled) {
    languages_enabled_in_pref_.Append(lang);
  } else {
    languages_enabled_in_pref_.EraseValue(base::Value(lang));
  }
}

bool ReadAloudAppModel::IsHighlightOn() {
  return highlight_granularity_ ==
         static_cast<int>(read_anything::mojom::HighlightGranularity::kOn);
}
