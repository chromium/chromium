// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {
namespace {

const char* EnumString(SavePageRequest::AutoFetchNotificationState value) {
  switch (value) {
    case SavePageRequest::AutoFetchNotificationState::kUnknown:
      return "kUnknown";
    case SavePageRequest::AutoFetchNotificationState::kShown:
      return "kShown";
  }
}
}  // namespace

std::string SavePageRequest::ToString() const {
  base::Value::Dict result;
  result.Set("request_id", static_cast<int>(request_id_));
  result.Set("url", url_.spec());
  result.Set("client_id", client_id_.ToString());
  result.Set(
      "creation_time",
      static_cast<int>(creation_time_.ToDeltaSinceWindowsEpoch().InSeconds()));
  result.Set("started_attempt_count", started_attempt_count_);
  result.Set("completed_attempt_count", completed_attempt_count_);
  result.Set("last_attempt_time",
             static_cast<int>(
                 last_attempt_time_.ToDeltaSinceWindowsEpoch().InSeconds()));
  result.Set("user_requested", user_requested_);
  result.Set("state", static_cast<int>(state_));
  result.Set("fail_state", static_cast<int>(fail_state_));
  result.Set("pending_state", static_cast<int>(pending_state_));
  result.Set("original_url", original_url_.spec());
  result.Set("request_origin", request_origin_);
  result.Set("auto_fetch_notification_state",
             EnumString(auto_fetch_notification_state_));

  std::string result_string;
  base::JSONWriter::Write(result, &result_string);
  return result_string;
}

// Implemented in test_util.cc.
std::ostream& operator<<(std::ostream& out,
                         SavePageRequest::AutoFetchNotificationState value) {
  return out << EnumString(value);
}

std::ostream& operator<<(std::ostream& out,
                         const Offliner::RequestStatus& value) {
  return out << Offliner::RequestStatusToString(value);
}

}  // namespace offline_pages
