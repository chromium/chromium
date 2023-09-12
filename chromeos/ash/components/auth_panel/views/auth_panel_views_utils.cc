// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/views/auth_panel_views_utils.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_textfield.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace ash {

void ConfigureAuthTextField(SystemTextfield* textfield) {
  textfield->SetBorder(nullptr);
  textfield->SetTextColorId(kColorAshTextColorPrimary);
  textfield->SetBackground(nullptr);
  textfield->SetPlaceholderTextColorId(kColorAshTextColorSecondary);
}

}  // namespace ash
