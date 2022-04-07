// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

// Button in the toolbar bar that displays the extensions that requests access,
// and grants them access.
class ExtensionsRequestAccessButton : public ToolbarButton {
 public:
  ExtensionsRequestAccessButton();
  ExtensionsRequestAccessButton(const ExtensionsRequestAccessButton&) = delete;
  const ExtensionsRequestAccessButton& operator=(
      const ExtensionsRequestAccessButton&) = delete;
  ~ExtensionsRequestAccessButton() override;

  // Updates the label based on the `extensions_requesting_access_count`. This
  // should only be called if there is at least one extension requesting access.
  void UpdateLabel(int extensions_requesting_access_count);

 private:
  void OnButtonPressed();
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
