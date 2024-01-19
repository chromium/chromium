// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_UTILS_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_UTILS_H_

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"

class Profile;

// This is used across Chrome Labs classes to check if a feature is valid.
bool IsChromeLabsFeatureValid(const LabInfo& lab, Profile* profile);

// Adds new experiments to PrefService and cleans up preferences for
// experiments that are no longer featured.
void UpdateChromeLabsNewBadgePrefs(Profile* profile,
                                   const ChromeLabsModel* model);

// This will indicate whether any Chrome Labs UI element (toolbar button,
// menu item, etc..) be shown.
bool ShouldShowChromeLabsUI(const ChromeLabsModel* model, Profile* profile);

// This will return true if there are new experiments and they haven't yet been
// seen.
bool AreNewChromeLabsExperimentsAvailable(const ChromeLabsModel* model,
                                          Profile* profile);

// This returns true if Chrome Labs is enabled. 99% of clients on
// pre-stable channels will have Chrome Labs enabled by default.
bool IsChromeLabsEnabled();

#endif  //  CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_UTILS_H_
