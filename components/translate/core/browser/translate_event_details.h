// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EVENT_DETAILS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EVENT_DETAILS_H_

#include <string>

#include "base/time/time.h"

namespace translate {

struct TranslateEventDetails {
  TranslateEventDetails(std::string in_filename,
                        int in_line,
                        std::string in_message);

  // The time when this event was created.
  base::Time time;

  // The source filename where this event was created.
  std::string filename;

  // The source line in |filename| where this event was created.
  int line;

  // The message to show in event logs.
  std::string message;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EVENT_DETAILS_H_
