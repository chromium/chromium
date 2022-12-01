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
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey("request_id", request_id_);
  result.SetStringKey("url", url_.spec());
  result.SetStringKey("client_id", client_id_.ToString());
  result.SetIntKey("creation_time",
                   creation_time_.ToDeltaSinceWindowsEpoch().InSeconds());
  result.SetIntKey("started_attempt_count", started_attempt_count_);
  result.SetIntKey("completed_attempt_count", completed_attempt_count_);
  result.SetIntKey("last_attempt_time",
                   last_attempt_time_.ToDeltaSinceWindowsEpoch().InSeconds());
  result.SetBoolKey("user_requested", user_requested_);
  result.SetIntKey("state", static_cast<int>(state_));
  result.SetIntKey("fail_state", static_cast<int>(fail_state_));
  result.SetIntKey("pending_state", static_cast<int>(pending_state_));
  result.SetStringKey("original_url", original_url_.spec());
  result.SetStringKey("request_origin", request_origin_);
  result.SetStringKey("auto_fetch_notification_state",
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
