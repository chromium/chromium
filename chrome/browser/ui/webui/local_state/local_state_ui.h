// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class Value;
}

// Namespace for exposing the method for unit tests.
namespace internal {

// Removes elements from |prefs| where the key does not match any of the
// prefixes in |valid_prefixes|.
void FilterPrefs(const std::vector<std::string>& valid_prefixes,
                 base::Value& prefs);

}  // namespace internal

// Controller for chrome://local-state/ page.
class LocalStateUI : public content::WebUIController {
 public:
  explicit LocalStateUI(content::WebUI* web_ui);
  ~LocalStateUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalStateUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_
