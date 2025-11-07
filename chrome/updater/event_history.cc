// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>

#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#endif

namespace updater {
namespace {

base::Lock& GetLoggingLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

base::File& GetLogFile() {
  static base::NoDestructor<base::File> file;
  return *file;
}

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

void SetWorldReadablePermissions(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  // Grant read access to non-admin users.
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  if (!sd) {
    VPLOG(1) << "Failed to read security descriptor for " << path;
    return;
  }
  if (!sd->SetDaclEntry(base::win::Sid(base::win::WellKnownSid::kWorld),
                        base::win::SecurityAccessMode::kGrant,
                        FILE_GENERIC_READ, 0)) {
    VPLOG(1) << "Failed to grant read access to " << path;
    return;
  }
  if (!sd->WriteToFile(path, DACL_SECURITY_INFORMATION)) {
    VPLOG(1) << "Failed to write security descriptor for " << path;
  }
#else
  if (!base::SetPosixFilePermissions(
          path, base::FILE_PERMISSION_READ_BY_USER |
                    base::FILE_PERMISSION_WRITE_BY_USER |
                    base::FILE_PERMISSION_READ_BY_GROUP |
                    base::FILE_PERMISSION_READ_BY_OTHERS)) {
    VPLOG(1) << "Failed to set permissions on " << path;
  }
#endif
}

}  // namespace

void InitHistoryLogging(const base::FilePath& path) {
  base::AutoLock lock(GetLoggingLock());
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND;
#if BUILDFLAG(IS_WIN)
  // base::File opens with FILE_SHARE_READ and FILE_SHARE_WRITE by default if
  // exclusivity flags are not provided.
  flags |= base::File::FLAG_WIN_SHARE_DELETE;
#endif
  GetLogFile() = base::File(path, flags);

  if (GetUpdaterScope() == UpdaterScope::kSystem) {
    SetWorldReadablePermissions(path);
  }
}

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

void Event::Write() const {
  base::AutoLock lock(GetLoggingLock());
  if (!GetLogFile().IsValid()) {
    VLOG(1) << "Failed to write history event: logging not initialized";
    return;
  }

  std::optional<std::string> json = base::WriteJson(ToDict());
  if (!json) {
    VLOG(1) << "Failed to write history event: JSON serialization failed";
    return;
  }
  *json = base::StrCat({*json, "\n"});

  std::optional<size_t> bytes_written =
      GetLogFile().WriteAtCurrentPos(base::as_byte_span(*json));
  if (!bytes_written) {
    VPLOG(1) << "Failed to write history event";
    return;
  }

  if (*bytes_written != json->size()) {
    VLOG(1) << "Failed to write history event: unable to write complete JSON "
               "message. Wrote "
            << *bytes_written << " out of " << json->size() << " bytes";
    return;
  }

  VLOG(2) << "Emitted a " << event_type() << " " << BoundToString(bound())
          << " event to the history log";
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
