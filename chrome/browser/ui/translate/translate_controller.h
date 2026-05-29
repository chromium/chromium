// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_

#include <string>

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// A cross-platform interface to control Translate UI, registered as
// UnownedUserData on BrowserWindowInterface.
class TranslateController {
 public:
  DECLARE_USER_DATA(TranslateController);

  static TranslateController* From(BrowserWindowInterface* window);

  virtual void StartPartialTranslate(const std::string& source_language,
                                     const std::string& target_language,
                                     const std::u16string& text_selection) = 0;

 protected:
  virtual ~TranslateController() = default;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_
