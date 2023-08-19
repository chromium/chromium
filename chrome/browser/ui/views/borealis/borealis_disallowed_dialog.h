// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_DISALLOWED_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_DISALLOWED_DIALOG_H_

#include "chrome/browser/ash/borealis/borealis_features.h"

namespace views::borealis {

void ShowInstallerDisallowedDialog(
    ::borealis::BorealisFeatures::AllowStatus status);

void ShowLauncherDisallowedDialog(
    ::borealis::BorealisFeatures::AllowStatus status);
}  // namespace views::borealis

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_DISALLOWED_DIALOG_H_
