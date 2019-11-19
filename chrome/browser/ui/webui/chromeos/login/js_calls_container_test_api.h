// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_TEST_API_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_TEST_API_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class JSCallsContainer;

// Enables setting up expectations for incoming and outgoing JavaScript in
// OOBE/login. When constructed this automatically enables event recording;
// JavaScript messages from before this is constructed may not be captured.
//
// Messages are automatically dispatched to the mocks in the destructor.
class JSCallsContainerTestApi {
 public:
  explicit JSCallsContainerTestApi(JSCallsContainer* js_calls_container);
  ~JSCallsContainerTestApi();

  // |function| has arguments serialized to make matching easier. For example, a
  // valid invocation looks like
  //
  //   EXPECT_CALL(test_api,
  //               Outgoing("login.MyComponent.setUpdateRequired(true, 5)"));

  // Notification from WebUI to C++
  MOCK_METHOD1(Incoming, void(std::string function));
  // Notification from C++ to WebUI
  MOCK_METHOD1(Outgoing, void(std::string function));

 private:
  JSCallsContainer* const js_calls_container_;
  DISALLOW_COPY_AND_ASSIGN(JSCallsContainerTestApi);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_TEST_API_H_
