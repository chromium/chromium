// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_DISALLOWED_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_DISALLOWED_DIALOG_H_

#include "chrome/browser/ash/borealis/borealis_features.h"

namespace views::borealis {

void ShowInstallerDisallowedDialog(
    ::borealis::BorealisFeatures::AllowStatus status);

}

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_DISALLOWED_DIALOG_H_
