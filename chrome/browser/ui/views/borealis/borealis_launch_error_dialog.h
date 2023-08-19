// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_LAUNCH_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_LAUNCH_ERROR_DIALOG_H_

#include "chrome/browser/ash/borealis/borealis_metrics.h"

class Profile;

namespace views::borealis {

void ShowBorealisLaunchErrorView(Profile* profile,
                                 ::borealis::BorealisStartupResult error);

}

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_LAUNCH_ERROR_DIALOG_H_
