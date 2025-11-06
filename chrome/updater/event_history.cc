// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <atomic>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace updater {
namespace {

std::string BoundToString(Event::Bound bound) {
  switch (bound) {
    case Event::Bound::kStart:
      return "START";
    case Event::Bound::kEnd:
      return "END";
    case Event::Bound::kInstant:
      return "INSTANT";
  }
}

}  // namespace

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

base::Value::Dict Event::ToDict() const {
  base::Value::Dict dict =
      base::Value::Dict()
          .Set("eventType", event_type_)
          .Set("eventId", event_id_)
          .Set("deviceUptime", base::TimeDeltaToValue(device_uptime_))
          .Set("pid", pid_)
          .Set("processToken", process_token_)
          .Set("bound", BoundToString(bound_));
  if (!errors_.empty()) {
    base::Value::List errors;
    for (const auto& error : errors_) {
      errors.Append(error.ToDict());
    }
    dict.Set("errors", std::move(errors));
  }
  ToDictInternal(dict);
  return dict;
}

base::Value::Dict Event::Error::ToDict() const {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("category", category)
                               .Set("code", code)
                               .Set("extracode1", extracode1);
  return dict;
}

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

void InstallStartEvent::ToDictInternal(base::Value::Dict& dict) const {
  dict.Set("appId", app_id_);
}

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

void InstallEndEvent::ToDictInternal(base::Value::Dict& dict) const {
  if (version_) {
    dict.Set("version", *version_);
  }
}

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
