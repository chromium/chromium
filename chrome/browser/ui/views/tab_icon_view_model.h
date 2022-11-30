// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_MODEL_H_

namespace ui {
class ImageModel;
}  // namespace ui

// Classes implement this interface to provide state for the TabIconView.
class TabIconViewModel {
 public:
  // Returns true if the TabIconView should show a loading animation.
  virtual bool ShouldTabIconViewAnimate() const = 0;

  // Returns the favicon to display in the icon view
  virtual ui::ImageModel GetFaviconForTabIconView() = 0;

 protected:
  virtual ~TabIconViewModel() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_MODEL_H_
