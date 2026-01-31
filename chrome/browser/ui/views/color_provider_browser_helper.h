// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ui {
class ColorProviderSource;
}  // namespace ui

class TabStripModel;

// A TabStripModelObserver that ensures the WebContents in the TabStripModel
// observe the correct ColorProviderSource.
class ColorProviderBrowserHelper : public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(ColorProviderBrowserHelper);

  ColorProviderBrowserHelper(TabStripModel* tab_strip_model,
                             ui::ColorProviderSource* color_provider_source,
                             BrowserWindowInterface* browser);
  ColorProviderBrowserHelper(const ColorProviderBrowserHelper&) = delete;
  ColorProviderBrowserHelper& operator=(const ColorProviderBrowserHelper&) =
      delete;
  ~ColorProviderBrowserHelper() override;

  static ColorProviderBrowserHelper* From(
      BrowserWindowInterface* browser_window_interface);

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  ui::ColorProviderSource* color_provider_source() {
    return color_provider_source_;
  }
  const ui::ColorProviderSource* color_provider_source() const {
    return color_provider_source_;
  }

 private:
  const raw_ptr<TabStripModel> tab_strip_model_;
  const raw_ptr<ui::ColorProviderSource> color_provider_source_;
  ui::ScopedUnownedUserData<ColorProviderBrowserHelper> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLOR_PROVIDER_BROWSER_HELPER_H_
