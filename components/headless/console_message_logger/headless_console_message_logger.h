// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_CONSOLE_MESSAGE_LOGGER_HEADLESS_CONSOLE_MESSAGE_LOGGER_H_
#define COMPONENTS_HEADLESS_CONSOLE_MESSAGE_LOGGER_HEADLESS_CONSOLE_MESSAGE_LOGGER_H_

#include "content/public/browser/console_message.h"

namespace headless {

// Alternate console message logger that logs JS messages while in incognito
// mode which is how headless runs most of the time.
void LogConsoleMessage(blink::mojom::ConsoleMessageLevel log_level,
                       const std::u16string& message,
                       int32_t line_number,
                       bool is_builtin_component,
                       const std::u16string& source_id);

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_CONSOLE_MESSAGE_LOGGER_HEADLESS_CONSOLE_MESSAGE_LOGGER_H_
