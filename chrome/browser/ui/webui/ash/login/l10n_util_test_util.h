// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_TEST_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_TEST_UTIL_H_

#include <memory>
#include <string>

#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"

namespace ash {

class MockInputMethodManagerWithInputMethods
    : public input_method::MockInputMethodManagerImpl {
 public:
  MockInputMethodManagerWithInputMethods();

  MockInputMethodManagerWithInputMethods(
      const MockInputMethodManagerWithInputMethods&) = delete;
  MockInputMethodManagerWithInputMethods& operator=(
      const MockInputMethodManagerWithInputMethods&) = delete;

  ~MockInputMethodManagerWithInputMethods() override;

  void AddInputMethod(const std::string& id,
                      const std::string& raw_layout,
                      const std::string& language_code);

 private:
  input_method::InputMethodDescriptors descriptors_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_TEST_UTIL_H_
