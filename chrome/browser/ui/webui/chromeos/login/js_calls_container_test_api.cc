// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/js_calls_container_test_api.h"

#include <sstream>

#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"

namespace chromeos {

JSCallsContainerTestApi::JSCallsContainerTestApi(
    JSCallsContainer* js_calls_container)
    : js_calls_container_(js_calls_container) {
  // Try to avoid a race condition where events potentially get dropped because
  // recording hasn't been enabled yet.
  DCHECK(js_calls_container_->record_all_events_for_test() ||
         js_calls_container_->events()->empty());

  js_calls_container_->set_record_all_events_for_test();
}

JSCallsContainerTestApi::~JSCallsContainerTestApi() {
  auto build_function_string =
      [](const JSCallsContainer::Event& event) -> std::string {
    std::stringstream result;
    result << event.function_name;
    result << '(';
    bool first = true;
    for (const base::Value& value : event.arguments) {
      if (!first)
        result << ", ";
      first = false;
      result << value;
    }
    result << ')';
    return result.str();
  };

  for (const auto& event : *js_calls_container_->events()) {
    if (event.type == JSCallsContainer::Event::Type::kIncoming)
      Incoming(build_function_string(event));
    else
      Outgoing(build_function_string(event));
  }
}

}  // namespace chromeos
