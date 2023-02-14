// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_LOGGING_PARAMETERS_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_LOGGING_PARAMETERS_H_

#include <string>

#include "components/feed/core/v2/public/types.h"

namespace feedui {
class LoggingParameters;
}

namespace feed {
struct StreamModelUpdateRequest;

struct LoggingParameters {
  LoggingParameters();
  ~LoggingParameters();
  LoggingParameters(const LoggingParameters&);
  LoggingParameters(LoggingParameters&&);
  LoggingParameters& operator=(const LoggingParameters&);

  // User ID, if the user is signed-in.
  std::string email;
  // A unique ID for this client. Used for reliability logging.
  std::string client_instance_id;
  // Whether attention / interaction logging is enabled.
  bool logging_enabled = false;
  // Whether view actions may be recorded.
  bool view_actions_enabled = false;
  // EventID of the first page response.
  std::string root_event_id;

  bool operator==(const LoggingParameters& rhs) const;
};

LoggingParameters MakeLoggingParameters(
    const std::string client_instance_id,
    const StreamModelUpdateRequest& update_request);

LoggingParameters FromProto(const feedui::LoggingParameters& proto);
void ToProto(const LoggingParameters& logging_parameters,
             feedui::LoggingParameters& proto);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_LOGGING_PARAMETERS_H_
