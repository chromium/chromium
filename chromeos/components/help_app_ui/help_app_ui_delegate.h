// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_

#include <string>

#include "base/optional.h"

class PrefService;

namespace content {
class WebUIDataSource;
}

// A delegate which exposes browser functionality from //chrome to the help app
// ui page handler.
class HelpAppUIDelegate {
 public:
  virtual ~HelpAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://help-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual base::Optional<std::string> OpenFeedbackDialog() = 0;

  // Takes a WebUIDataSource, and adds device flags (e.g. board name) and
  // feature flags (e.g. Google Assistant).
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;

  // Opens OS Settings at the parental controls section.
  virtual void ShowParentalControls() = 0;

  // Gets locally stored users preferences and state.
  virtual PrefService* GetLocalState() = 0;
};

#endif  // CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
