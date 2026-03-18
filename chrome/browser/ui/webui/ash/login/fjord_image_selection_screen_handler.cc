// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_image_selection_screen_handler.h"

#include "chrome/grit/generated_resources.h"

namespace ash {

FjordImageSelectionScreenHandler::FjordImageSelectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FjordImageSelectionScreenHandler::~FjordImageSelectionScreenHandler() = default;

void FjordImageSelectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("fjordImageSelectionTitle", IDS_FJORD_IMAGE_SELECTION_TITLE);
  builder->Add("fjordImageSelectionMeetLabel",
               IDS_FJORD_IMAGE_SELECTION_MEET_LABEL);
  builder->Add("fjordImageSelectionZoomLabel",
               IDS_FJORD_IMAGE_SELECTION_ZOOM_LABEL);
  builder->Add("fjordImageSelectionNextButton",
               IDS_FJORD_IMAGE_SELECTION_NEXT_BUTTON_TEXT);
}

void FjordImageSelectionScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<FjordImageSelectionScreenView>
FjordImageSelectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
