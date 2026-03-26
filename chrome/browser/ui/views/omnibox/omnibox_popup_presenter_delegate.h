// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_DELEGATE_H_

namespace views {
class Widget;
}  // namespace views

class OmniboxPopupAimPresenter;
class OmniboxPopupFileSelector;

class OmniboxPopupPresenterDelegate {
 public:
  virtual ~OmniboxPopupPresenterDelegate() = default;
  virtual views::Widget* GetLocationBarWidget() = 0;
  virtual OmniboxPopupFileSelector* GetOmniboxPopupFileSelector() const = 0;
  virtual OmniboxPopupAimPresenter* GetOmniboxPopupAimPresenter() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_DELEGATE_H_
