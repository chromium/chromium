// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace new_tab_footer {

class NewTabFooterControllerObserver : public base::CheckedObserver {
 public:
  // Called when footer's visibility is updated.
  virtual void OnFooterVisibilityUpdated(bool visible) = 0;

 protected:
  ~NewTabFooterControllerObserver() override = default;
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_OBSERVER_H_
