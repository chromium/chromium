// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/l10n_util_test_util.h"

#include <vector>

#include "url/gurl.h"

namespace chromeos {

MockInputMethodManagerWithInputMethods::
    MockInputMethodManagerWithInputMethods() {
}

MockInputMethodManagerWithInputMethods::
    ~MockInputMethodManagerWithInputMethods() {
}

void MockInputMethodManagerWithInputMethods::AddInputMethod(
    const std::string& id,
    const std::string& raw_layout,
    const std::string& language_code) {
  std::vector<std::string> layouts;
  layouts.push_back(raw_layout);
  std::vector<std::string> languages;
  languages.push_back(language_code);
  descriptors_.push_back(input_method::InputMethodDescriptor(
      id, std::string(), std::string(), layouts, languages, true,
      GURL(), GURL()));
}

}  // namespace chromeos
