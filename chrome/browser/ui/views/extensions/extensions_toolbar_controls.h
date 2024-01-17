// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_

#include "base/memory/raw_ptr.h"
#include "url/origin.h"

class ExtensionsToolbarButton;
class ExtensionsRequestAccessButton;

class ExtensionsToolbarControls {
 public:
  explicit ExtensionsToolbarControls(
      const raw_ptr<ExtensionsToolbarButton> extensions_button,
      raw_ptr<ExtensionsRequestAccessButton> request_button);
  ExtensionsToolbarControls(const ExtensionsToolbarControls&) = delete;
  ExtensionsToolbarControls operator=(const ExtensionsToolbarControls&) =
      delete;
  ~ExtensionsToolbarControls();

  // Hides the confirmation message in the request access button.
  void ResetConfirmation();

  // Returns whether the button is showing a confirmation message.
  bool IsShowingConfirmation() const;

  // Returns whether the button is showing a confirmation message for `origin`.
  bool IsShowingConfirmationFor(const url::Origin& origin) const;

 private:
  const raw_ptr<ExtensionsRequestAccessButton> request_access_button_;
  const raw_ptr<ExtensionsToolbarButton> extensions_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
