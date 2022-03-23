// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include <utility>

#include "content/public/browser/web_ui.h"

namespace chromeos {

JSCallsContainer::JSCallsContainer() = default;

JSCallsContainer::~JSCallsContainer() = default;

void JSCallsContainer::ExecuteDeferredJSCalls(content::WebUI* web_ui) {
  DCHECK(!is_initialized());
  is_initialized_ = true;

  auto events = std::exchange(events_, {});
  // Execute recorded outgoing events.
  for (auto& event : events) {
    std::move(event).Run(web_ui);
  }
}

}  // namespace chromeos
