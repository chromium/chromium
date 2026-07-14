// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/theme_utils.h"

#include <optional>
#include <string>

#include "components/themes/ntp_custom_background_service_constants.h"

namespace themes {

base::DictValue GetBackgroundDictFromProto(
    const sync_pb::NtpCustomBackground& ntp_background) {
  base::DictValue dict;
  if (ntp_background.has_url()) {
    dict.Set(kNtpCustomBackgroundURL, ntp_background.url());
  }
  if (ntp_background.has_attribution_line_1()) {
    dict.Set(kNtpCustomBackgroundAttributionLine1,
             ntp_background.attribution_line_1());
  }
  if (ntp_background.has_attribution_line_2()) {
    dict.Set(kNtpCustomBackgroundAttributionLine2,
             ntp_background.attribution_line_2());
  }
  if (ntp_background.has_attribution_action_url()) {
    dict.Set(kNtpCustomBackgroundAttributionActionURL,
             ntp_background.attribution_action_url());
  }
  if (ntp_background.has_collection_id()) {
    dict.Set(kNtpCustomBackgroundCollectionId, ntp_background.collection_id());
  }
  if (ntp_background.has_resume_token()) {
    dict.Set(kNtpCustomBackgroundResumeToken, ntp_background.resume_token());
  }
  if (ntp_background.has_refresh_timestamp_unix_epoch_seconds()) {
    dict.Set(kNtpCustomBackgroundRefreshTimestamp,
             static_cast<int>(
                 ntp_background.refresh_timestamp_unix_epoch_seconds()));
  }
  if (ntp_background.has_main_color()) {
    dict.Set(kNtpCustomBackgroundMainColor,
             static_cast<int>(ntp_background.main_color()));
  }
  return dict;
}

sync_pb::NtpCustomBackground GetProtoFromBackgroundDict(
    const base::DictValue& dict) {
  sync_pb::NtpCustomBackground ntp_background;
  if (const std::string* value = dict.FindString(kNtpCustomBackgroundURL)) {
    ntp_background.set_url(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionLine1)) {
    ntp_background.set_attribution_line_1(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionLine2)) {
    ntp_background.set_attribution_line_2(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionActionURL)) {
    ntp_background.set_attribution_action_url(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundCollectionId)) {
    ntp_background.set_collection_id(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundResumeToken)) {
    ntp_background.set_resume_token(*value);
  }
  if (std::optional<int> value =
          dict.FindInt(kNtpCustomBackgroundRefreshTimestamp)) {
    ntp_background.set_refresh_timestamp_unix_epoch_seconds(*value);
  }
  if (std::optional<int> value = dict.FindInt(kNtpCustomBackgroundMainColor)) {
    ntp_background.set_main_color(*value);
  }
  return ntp_background;
}

}  // namespace themes
