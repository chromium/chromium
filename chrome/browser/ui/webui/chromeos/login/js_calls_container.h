// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/values.h"

namespace content {
class WebUI;
}

namespace chromeos {

// A helper class to store deferred Javascript calls, shared by subclasses of
// BaseWebUIHandler.
class JSCallsContainer {
 public:
  using Event = base::OnceCallback<void(content::WebUI* web_ui)>;

  JSCallsContainer();
  ~JSCallsContainer();
  JSCallsContainer(const JSCallsContainer&) = delete;
  JSCallsContainer& operator=(const JSCallsContainer&) = delete;

  // Used to decide whether the JS call should be deferred.
  bool is_initialized() const { return is_initialized_; }

  // Recorded events. This is mutable and can be modified.
  std::vector<Event>* events() { return &events_; }

  // Executes Javascript calls that were deferred while the instance was not
  // initialized yet.
  void ExecuteDeferredJSCalls(content::WebUI* web_ui);

 private:
  // Whether the instance is initialized.
  //
  // The instance becomes initialized after the corresponding message is
  // received from Javascript side.
  bool is_initialized_ = false;

  std::vector<Event> events_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_
