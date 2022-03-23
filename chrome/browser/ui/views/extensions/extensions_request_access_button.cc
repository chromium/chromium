// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include <memory>

#include "base/bind.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionsRequestAccessButton::ExtensionsRequestAccessButton()
    : ToolbarButton(
          base::BindRepeating(&ExtensionsRequestAccessButton::OnButtonPressed,
                              base::Unretained(this))) {
  // TODO(crbug.com/1239772): Set the label and background color without borders
  // separately to match the mocks. For now, using SetHighlight to display that
  // adds a border and highlight color in addition to the label.
  absl::optional<SkColor> color;
  SetHighlight(l10n_util::GetStringUTF16(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON),
               color);
}

ExtensionsRequestAccessButton::~ExtensionsRequestAccessButton() = default;

// TODO(crbug.com/1239772): Grant access to all the extensions requesting access
// when the button is pressed.
void ExtensionsRequestAccessButton::OnButtonPressed() {}
