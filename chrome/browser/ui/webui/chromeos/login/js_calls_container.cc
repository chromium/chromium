// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"

#include "content/public/browser/web_ui.h"

namespace chromeos {

JSCallsContainer::Event::Event(Type type,
                               const std::string& function_name,
                               std::vector<base::Value>&& arguments)
    : type(type),
      function_name(function_name),
      arguments(std::move(arguments)) {}

JSCallsContainer::Event::~Event() = default;

JSCallsContainer::Event::Event(Event&&) = default;

JSCallsContainer::JSCallsContainer() = default;

JSCallsContainer::~JSCallsContainer() = default;

void JSCallsContainer::ExecuteDeferredJSCalls(content::WebUI* web_ui) {
  DCHECK(!is_initialized());
  is_initialized_ = true;

  // Execute recorded outgoing events.
  for (const auto& event : events_) {
    if (event.type != Event::Type::kOutgoing)
      continue;

    // event.arguments is of type std::vector<base::Value>, but
    // CallJavascriptFunctionUnsafe requires std::vector<const base::Value*>.
    // Construct the new vector of pointers from the existing data.
    std::vector<const base::Value*> args;
    args.reserve(event.arguments.size());
    auto* bp = event.arguments.data();
    for (size_t i = 0; i < event.arguments.size(); ++i)
      args.push_back(bp + i);
    web_ui->CallJavascriptFunctionUnsafe(event.function_name, args);
  }

  // Reduce memory usage / drop potentially sensitive data if we're not
  // recording for a test.
  if (!record_all_events_for_test_)
    events_.clear();
}

}  // namespace chromeos
