// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_event_details.h"

#include <utility>

namespace translate {

TranslateEventDetails::TranslateEventDetails(std::string in_filename,
                                             int in_line,
                                             std::string in_message)
    : filename(std::move(in_filename)),
      line(in_line),
      message(std::move(in_message)) {
  time = base::Time::Now();
}

}  // namespace translate
