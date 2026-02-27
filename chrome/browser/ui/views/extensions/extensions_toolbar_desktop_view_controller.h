// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_DESKTOP_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_DESKTOP_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

class Browser;
class ExtensionsToolbarDesktop;

class ExtensionsToolbarDesktopViewController final
    : public TabStripModelObserver {
 public:
  // Flex behavior precedence for the container's views.
  static constexpr int kFlexOrderExtensionsButton = 1;
  static constexpr int kFlexOrderRequestAccessButton = 2;
  static constexpr int kFlexOrderActionView = 3;

  ExtensionsToolbarDesktopViewController(
      Browser* browser,
      ExtensionsToolbarDesktop* extensions_container);
  ExtensionsToolbarDesktopViewController(
      const ExtensionsToolbarDesktopViewController&) = delete;
  const ExtensionsToolbarDesktopViewController& operator=(
      const ExtensionsToolbarDesktopViewController&) = delete;
  ~ExtensionsToolbarDesktopViewController() override;

  // Updates the flex layout rules for the extension toolbar container to have
  // views::MinimumFlexSizeRule::kPreferred when WindowControlsOverlay (WCO) is
  // toggled on for PWAs. Otherwise the extensions icon does not stay visible as
  // it is not considered for during the calculation of the preferred size of
  // it's parent (in the case of WCO PWAs, WebAppFrameToolbarView).
  void WindowControlsOverlayEnabledChanged(bool enabled);

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  const raw_ptr<Browser> browser_;

  raw_ptr<ExtensionsToolbarDesktop> extensions_container_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_DESKTOP_VIEW_CONTROLLER_H_
