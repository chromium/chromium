// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_app_model.h"

ReadAloudAppModel::ReadAloudAppModel() = default;

ReadAloudAppModel::~ReadAloudAppModel() = default;

void ReadAloudAppModel::OnSettingsRestoredFromPrefs(double speech_rate) {
  speech_rate_ = speech_rate;
}
