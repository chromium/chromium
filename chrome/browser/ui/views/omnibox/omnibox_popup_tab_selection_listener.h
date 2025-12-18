// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_TAB_SELECTION_LISTENER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_TAB_SELECTION_LISTENER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

class OmniboxPopupTabSelectionListener : public TabStripModelObserver {
 public:
  OmniboxPopupTabSelectionListener(
      base::WeakPtr<WebUIContentsWrapper::Host> host,
      TabStripModel* tab_strip_model);
  OmniboxPopupTabSelectionListener(const OmniboxPopupTabSelectionListener&) =
      delete;
  OmniboxPopupTabSelectionListener& operator=(
      const OmniboxPopupTabSelectionListener&) = delete;
  ~OmniboxPopupTabSelectionListener() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  base::WeakPtr<WebUIContentsWrapper::Host> host_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_TAB_SELECTION_LISTENER_H_
