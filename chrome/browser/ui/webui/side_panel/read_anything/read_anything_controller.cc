// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_controller.h"

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model)
    : model_(model) {}

void ReadAnythingController::OnFontChoiceChanged(int new_choice) {
  model_->SetSelectedFontIndex(new_choice);
  model_->NotifyFontNameUpdated();
}

ReadAnythingController::~ReadAnythingController() = default;
