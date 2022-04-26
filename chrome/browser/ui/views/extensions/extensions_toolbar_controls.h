// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "content/public/browser/web_contents.h"

class ExtensionsToolbarButton;
class ExtensionsRequestAccessButton;
class ToolbarActionViewController;

class ExtensionsToolbarControls : public ToolbarIconContainerView {
 public:
  METADATA_HEADER(ExtensionsToolbarControls);

  explicit ExtensionsToolbarControls(
      std::unique_ptr<ExtensionsToolbarButton> extensions_button,
      std::unique_ptr<ExtensionsToolbarButton> site_access_button,
      std::unique_ptr<ExtensionsRequestAccessButton> request_button);
  ExtensionsToolbarControls(const ExtensionsToolbarControls&) = delete;
  ExtensionsToolbarControls operator=(const ExtensionsToolbarControls&) =
      delete;
  ~ExtensionsToolbarControls() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }

  // Methods for testing.
  ExtensionsToolbarButton* site_access_button_for_testing() const {
    return site_access_button_;
  }
  ExtensionsRequestAccessButton* request_access_button_for_testing() const {
    return request_access_button_;
  }

  // Updates `site_access_button_` visibility to the given one.
  void UpdateSiteAccessButtonVisibility(bool visibility);

  // Updates `request_access_button_` visibility and content based on the given
  // `count_requesting_extensions`.
  void UpdateRequestAccessButton(
      std::vector<ToolbarActionViewController*> extensions_requesting_access);

  // Resets the layout since layout animation does not handle host view
  // visibility changing. This should be called after any visibility changes.
  void ResetLayout();

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  const raw_ptr<ExtensionsRequestAccessButton> request_access_button_;
  const raw_ptr<ExtensionsToolbarButton> site_access_button_;
  const raw_ptr<ExtensionsToolbarButton> extensions_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
