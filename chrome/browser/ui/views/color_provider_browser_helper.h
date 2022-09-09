// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Browser;
class TabStripModel;

// A TabStripModelObserver that ensures the WebContents in the TabStripModel
// observe the correct ColorProviderSource.
class ColorProviderBrowserHelper
    : public BrowserUserData<ColorProviderBrowserHelper>,
      public TabStripModelObserver {
 public:
  ColorProviderBrowserHelper(const ColorProviderBrowserHelper&) = delete;
  ColorProviderBrowserHelper& operator=(const ColorProviderBrowserHelper&) =
      delete;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  friend class BrowserUserData<ColorProviderBrowserHelper>;

  explicit ColorProviderBrowserHelper(Browser* browser);

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_
