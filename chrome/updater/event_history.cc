// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <optional>
#include <string>

#include "base/time/time.h"

namespace updater {

Event::~Event() = default;

Event::Event(const std::string& event_type,
             Bound bound,
             const CommonFields& common_fields)
    : event_type_(event_type),
      bound_(bound),
      event_id_(common_fields.event_id),
      device_uptime_(common_fields.device_uptime),
      pid_(common_fields.pid),
      process_token_(common_fields.process_token),
      errors_(common_fields.errors) {}

Event::CommonFields::CommonFields(std::string event_id,
                                  base::TimeDelta device_uptime,
                                  int pid,
                                  const std::string& process_token,
                                  const std::vector<Error>& errors)
    : event_id(event_id),
      device_uptime(device_uptime),
      pid(pid),
      process_token(process_token),
      errors(errors) {}
Event::CommonFields::CommonFields(const CommonFields&) = default;
Event::CommonFields& Event::CommonFields::operator=(const CommonFields&) =
    default;
Event::CommonFields::CommonFields(CommonFields&&) = default;
Event::CommonFields& Event::CommonFields::operator=(CommonFields&&) = default;
Event::CommonFields::~CommonFields() = default;

InstallStartEvent::InstallStartEvent(const CommonFields& common_fields,
                                     const std::string& app_id)
    : Event("INSTALL", Bound::kStart, common_fields), app_id_(app_id) {}

InstallStartEvent::~InstallStartEvent() = default;

InstallEndEvent::InstallEndEvent(const CommonFields& common_fields,
                                 std::optional<std::string> version)
    : Event("INSTALL", Bound::kEnd, common_fields), version_(version) {}
InstallEndEvent::~InstallEndEvent() = default;

}  // namespace updater
