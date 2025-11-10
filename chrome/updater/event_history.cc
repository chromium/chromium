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
#include "base/numerics/clamped_math.h"
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

void InitHistoryLogging(const base::FilePath& path,
                        size_t max_file_size_bytes) {
  base::AutoLock lock(GetLoggingLock());

  if (GetLogFile().IsValid()) {
    GetLogFile().Close();
  }

  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (file_size &&
      base::ClampedNumeric<size_t>(*file_size) > max_file_size_bytes) {
    const base::FilePath rotated_path =
        path.AddExtension(FILE_PATH_LITERAL(".old"));
    base::DeleteFile(rotated_path);
    if (!base::Move(path, rotated_path)) {
      VPLOG(1) << "Failed to rotate event history log";
    }
  }

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

void WriteHistoryEvent(const base::Value::Dict& event) {
  base::AutoLock lock(GetLoggingLock());
  if (!GetLogFile().IsValid()) {
    VLOG(1) << "Failed to write history event: logging not initialized";
    return;
  }

  std::optional<std::string> json = base::WriteJson(event);
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

  const std::string* event_id = event.FindString("eventId");
  const std::string* bound = event.FindString("bound");
  VLOG(2) << "Emitted a " << (event_id ? *event_id : "(unknown)") << " "
          << (bound ? *bound : "INSTANT") << " event to the history log";
}

base::Value::Dict HistoryEventError::ToDict() const {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("category", category)
                               .Set("code", code)
                               .Set("extracode1", extracode1);
  return dict;
}

InstallStartEvent::InstallStartEvent() = default;
InstallStartEvent::~InstallStartEvent() = default;

InstallStartEvent& InstallStartEvent::SetAppId(const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

std::optional<base::Value::Dict> InstallStartEvent::BuildInternal(
    base::Value::Dict event) const {
  if (app_id_.empty()) {
    VLOG(1) << "Failed to build InstallStartEvent, app_id is empty";
    return std::nullopt;
  }
  event.Set("eventType", "INSTALL");
  event.Set("bound", "START");
  event.Set("appId", app_id_);
  return event;
}

InstallEndEvent::InstallEndEvent() = default;
InstallEndEvent::~InstallEndEvent() = default;

InstallEndEvent& InstallEndEvent::SetVersion(const std::string& version) {
  version_ = version;
  return *this;
}

std::optional<base::Value::Dict> InstallEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "INSTALL");
  event.Set("bound", "END");
  if (version_) {
    event.Set("version", *version_);
  }
  return event;
}

}  // namespace updater
