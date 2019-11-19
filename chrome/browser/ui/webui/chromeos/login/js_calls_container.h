// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"

namespace content {
class WebUI;
}

namespace chromeos {

// A helper class to store deferred Javascript calls, shared by subclasses of
// BaseWebUIHandler.
class JSCallsContainer {
 public:
  // An event is a message/JS call to or from WebUI.
  struct Event {
    enum class Type {
      // This event was sent from C++ to JS.
      kOutgoing,
      // This event was sent from JS to C++.
      kIncoming,
    };

    Event(Type type,
          const std::string& function_name,
          std::vector<base::Value>&& arguments);
    ~Event();
    Event(Event&&);
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    Type type;
    std::string function_name;
    std::vector<base::Value> arguments;
  };

  JSCallsContainer();
  ~JSCallsContainer();
  JSCallsContainer(const JSCallsContainer&) = delete;
  JSCallsContainer& operator=(const JSCallsContainer&) = delete;

  // Used to decide whether the JS call should be deferred.
  bool is_initialized() const { return is_initialized_; }

  // Enable event recording.
  void set_record_all_events_for_test() { record_all_events_for_test_ = true; }

  // If true then all JS calls should be recorded.
  bool record_all_events_for_test() const {
    return record_all_events_for_test_;
  }

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

  // Decide if incoming and outgoing JS calls should be recorded. Recording
  // should only be used for tests.
  bool record_all_events_for_test_ = false;

  std::vector<Event> events_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_JS_CALLS_CONTAINER_H_
