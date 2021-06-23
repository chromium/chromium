// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_UTILS_H_

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"

class Profile;

// This is used across Chrome Labs classes to check if a feature is valid.
bool IsChromeLabsFeatureValid(const LabInfo& lab, Profile* profile);

#endif  //  CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_UTILS_H_
