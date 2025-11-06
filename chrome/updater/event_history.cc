// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <atomic>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace updater {

std::string GenerateEventId() {
  static std::atomic_uint counter(0);
  return base::NumberToString(counter++);
}

const std::string& GetProcessToken() {
  static const base::NoDestructor<std::string> process_token(
      base::Base64Encode(base::RandBytesAsString(16)));
  return *process_token;
}

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

InstallStartEvent::Builder::Builder() = default;
InstallStartEvent::Builder::~Builder() = default;

InstallStartEvent::Builder& InstallStartEvent::Builder::SetAppId(
    const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

std::unique_ptr<InstallStartEvent> InstallStartEvent::Builder::Build() const {
  std::optional<CommonFields> common_fields = BuildCommonFields();
  if (!common_fields) {
    VLOG(1) << "Failed to build event, invalid common fields";
    return nullptr;
  }
  if (app_id_.empty()) {
    VLOG(1) << "Failed to build InstallStartEvent, app_id is empty";
    return nullptr;
  }
  return base::WrapUnique(
      new InstallStartEvent(*std::move(common_fields), app_id_));
}

InstallEndEvent::InstallEndEvent(const CommonFields& common_fields,
                                 std::optional<std::string> version)
    : Event("INSTALL", Bound::kEnd, common_fields), version_(version) {}
InstallEndEvent::~InstallEndEvent() = default;

InstallEndEvent::Builder::Builder() = default;
InstallEndEvent::Builder::~Builder() = default;

InstallEndEvent::Builder& InstallEndEvent::Builder::SetVersion(
    const std::string& version) {
  version_ = version;
  return *this;
}

std::unique_ptr<InstallEndEvent> InstallEndEvent::Builder::Build() const {
  std::optional<CommonFields> common_fields = BuildCommonFields();
  if (!common_fields) {
    VLOG(1) << "Failed to build event, invalid common fields";
    return nullptr;
  }
  return base::WrapUnique(
      new InstallEndEvent(*std::move(common_fields), version_));
}

}  // namespace updater
