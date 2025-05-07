// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_

#include "components/tabs/public/tab_interface.h"

namespace new_tab_footer {

// Class used to manage the state of the new tab footer.
class NewTabFooterController {
 public:
  explicit NewTabFooterController(tabs::TabInterface* tab);
  NewTabFooterController(const NewTabFooterController&) = delete;
  NewTabFooterController& operator=(const NewTabFooterController&) = delete;

 private:
  const raw_ptr<tabs::TabInterface> tab_;
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
