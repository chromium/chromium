// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/synchronized_minidump_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/crash/linux/dump_info.h"

// if |cond| is false, returns |retval|.
#define RCHECK(cond, retval) \
  do {                       \
    if (!(cond)) {           \
      return (retval);       \
    }                        \
  } while (0)

namespace chromecast {

namespace {

// Allows overriding default placement of minidumps in $HOME.
const char kMinidumpPathSwitch[] = "minidump-path";

const char kLockfileName[] = "lockfile";
const char kMetadataName[] = "metadata";
const char kMinidumpsDir[] = "minidumps";

const char kLockfileRatelimitKey[] = "ratelimit";
const char kLockfileRatelimitPeriodStartKey[] = "period_start";
const char kLockfileRatelimitPeriodDumpsKey[] = "period_dumps";
const uint64_t kLockfileNumRatelimitParams = 2;

base::FilePath GetMinidumpPath() {
  base::FilePath result =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kMinidumpPathSwitch);
  if (result.empty()) {
    result = GetHomePathASCII(kMinidumpsDir);
  }
  return result;
}

// Gets the ratelimit parameter dictionary given a deserialized |metadata|.
// Returns nullptr if invalid.
base::Value::Dict* GetRatelimitParams(
    std::optional<base::Value::Dict>& metadata) {
  if (!metadata)
    return nullptr;
  return metadata->FindDict(kLockfileRatelimitKey);
}

// Returns the time of the current ratelimit period's start in |metadata|.
// Returns base::Time() if an error occurs.
base::Time GetRatelimitPeriodStart(std::optional<base::Value::Dict>& metadata) {
  base::Value::Dict* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, base::Time());

  std::optional<double> seconds =
      ratelimit_params->FindDouble(kLockfileRatelimitPeriodStartKey);
  RCHECK(seconds, base::Time());

  // Return value of 0 indicates "not initialized", so we need to explicitly
  // check for it and return time_t = 0 equivalent.
  return *seconds ? base::Time::FromSecondsSinceUnixEpoch(*seconds)
                  : base::Time::UnixEpoch();
}

// Sets the time of the current ratelimit period's start in |metadata| to
// |period_start|. Returns true on success, false on error.
bool SetRatelimitPeriodStart(std::optional<base::Value::Dict>& metadata,
                             base::Time period_start) {
  DCHECK(!period_start.is_null());

  base::Value::Dict* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, false);

  ratelimit_params->Set(kLockfileRatelimitPeriodStartKey,
                        period_start.InSecondsFSinceUnixEpoch());
  return true;
}

// Gets the number of dumps added to |metadata| in the current ratelimit
// period. Returns < 0 on error.
int GetRatelimitPeriodDumps(std::optional<base::Value::Dict>& metadata) {
  base::Value::Dict* ratelimit_params = GetRatelimitParams(metadata);
  if (!ratelimit_params)
    return -1;
  std::optional<int> period_dumps =
      ratelimit_params->FindInt(kLockfileRatelimitPeriodDumpsKey);
  return period_dumps.value_or(-1);
}

// Sets the current ratelimit period's number of dumps in |metadata| to
// |period_dumps|. Returns true on success, false on error.
bool SetRatelimitPeriodDumps(std::optional<base::Value::Dict>& metadata,
                             int period_dumps) {
  DCHECK_GE(period_dumps, 0);

  base::Value::Dict* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, false);

  ratelimit_params->Set(kLockfileRatelimitPeriodDumpsKey, period_dumps);

  return true;
}

// Returns true if |metadata| contains valid metadata, false otherwise.
bool ValidateMetadata(std::optional<base::Value::Dict>& metadata) {
  RCHECK(metadata, false);

  // Validate ratelimit params
  base::Value::Dict* ratelimit_params = GetRatelimitParams(metadata);

  return ratelimit_params &&
         ratelimit_params->size() == kLockfileNumRatelimitParams &&
         !GetRatelimitPeriodStart(metadata).is_null() &&
         GetRatelimitPeriodDumps(metadata) >= 0;
}

// Calls flock on valid file descriptor |fd| with flag |flag|. Returns true
// on success, false on failure.
bool CallFlockOnFileWithFlag(const int fd, int flag) {
  int ret = -1;
  if ((ret = HANDLE_EINTR(flock(fd, flag))) < 0)
    PLOG(ERROR) << "Error locking " << fd;

  return !ret;
}

int OpenAndLockFile(const base::FilePath& path, bool write) {
  int fd = -1;
  const char* file = path.value().c_str();

  if ((fd = open(file, write ? O_RDWR : O_RDONLY)) < 0) {
    PLOG(ERROR) << "Error opening " << file;
  } else if (!CallFlockOnFileWithFlag(fd, LOCK_EX)) {
    close(fd);
    fd = -1;
  }

  return fd;
}

bool UnlockAndCloseFile(const int fd) {
  if (!CallFlockOnFileWithFlag(fd, LOCK_UN))
    return false;
  return !close(fd);
}

}  // namespace

// One day
const int SynchronizedMinidumpManager::kRatelimitPeriodSeconds = 24 * 3600;
const int SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps = 100;

SynchronizedMinidumpManager::SynchronizedMinidumpManager()
    : dump_path_(GetMinidumpPath()),
      lockfile_path_(dump_path_.Append(kLockfileName).value()),
      metadata_path_(dump_path_.Append(kMetadataName).value()),
      lockfile_fd_(-1) {}

SynchronizedMinidumpManager::~SynchronizedMinidumpManager() {
  // Release the lock if held.
  ReleaseLockFile();
}

// TODO(slan): Move some of this pruning logic to ReleaseLockFile?
int SynchronizedMinidumpManager::GetNumDumps(bool delete_all_dumps) {
  int num_dumps = 0;

  base::DirReaderPosix reader(dump_path_.value().c_str());
  if (!reader.IsValid()) {
    LOG(ERROR) << "Unable to open directory " << dump_path_.value();
    return 0;
  }

  while (reader.Next()) {
    if (strcmp(reader.name(), ".") == 0 || strcmp(reader.name(), "..") == 0)
      continue;

    const base::FilePath dump_file(dump_path_.Append(reader.name()));
    // If file cannot be found, skip.
    if (!base::PathExists(dump_file))
      continue;

    // Do not count |lockfile_path_| and |metadata_path_|.
    if (lockfile_path_ != dump_file && metadata_path_ != dump_file) {
      ++num_dumps;
      if (delete_all_dumps) {
        LOG(INFO) << "Removing " << reader.name()
                  << "which was not in the lockfile";
        if (!base::DeleteFile(dump_file))
          PLOG(INFO) << "Removing " << dump_file.value() << " failed";
      }
    }
  }

  return num_dumps;
}

bool SynchronizedMinidumpManager::AcquireLockAndDoWork() {
  bool success = false;
  if (AcquireLockFile()) {
    success = DoWork();
    ReleaseLockFile();
  }
  return success;
}

bool SynchronizedMinidumpManager::AcquireLockFile() {
  DCHECK_LT(lockfile_fd_, 0);
  // Make the directory for the minidumps if it does not exist.
  base::File::Error error;
  if (!CreateDirectoryAndGetError(dump_path_, &error)) {
    LOG(ERROR) << "Failed to create directory " << dump_path_.value()
               << ". error = " << error;
    return false;
  }

  // Open the lockfile. Create it if it does not exist.
  base::File lockfile(lockfile_path_, base::File::FLAG_OPEN_ALWAYS);

  // If opening or creating the lockfile failed, we don't want to proceed
  // with dump writing for fear of exhausting up system resources.
  if (!lockfile.IsValid()) {
    LOG(ERROR) << "open lockfile failed " << lockfile_path_.value();
    return false;
  }

  if ((lockfile_fd_ = OpenAndLockFile(lockfile_path_, false)) < 0) {
    ReleaseLockFile();
    return false;
  }

  // The lockfile is open and locked. Parse it to provide subclasses with a
  // record of all the current dumps.
  bool create_lockfiles = false;
  if (!base::PathExists(metadata_path_)) {
    LOG(INFO) << "Metadata doesn't exist.";
    create_lockfiles = true;
  } else if (!ParseFiles()) {
    LOG(ERROR) << "Lockfile did not parse correctly. ";
    create_lockfiles = true;
  }
  if (create_lockfiles && (!InitializeFiles() || !ParseFiles())) {
    LOG(ERROR) << "Failed to create a new lock file!";
    ReleaseLockFile();
    return false;
  }

  DCHECK(dumps_);
  DCHECK(metadata_);

  // We successfully have acquired the lock.
  return true;
}

bool SynchronizedMinidumpManager::ParseFiles() {
  DCHECK_GE(lockfile_fd_, 0);
  DCHECK(!dumps_);
  DCHECK(!metadata_);

  std::string lockfile;
  RCHECK(ReadFileToString(lockfile_path_, &lockfile), false);

  std::vector<std::string> lines = base::SplitString(
      lockfile, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::Value::List dumps;

  // Validate dumps
  for (const std::string& line : lines) {
    if (line.size() == 0)
      continue;
    std::optional<base::Value> dump_info = base::JSONReader::Read(line);
    RCHECK(dump_info.has_value(), false);
    DumpInfo info(&dump_info.value());
    RCHECK(info.valid(), false);
    dumps.Append(std::move(dump_info.value()));
  }

  JSONFileValueDeserializer deserializer(metadata_path_);
  int error_code = -1;
  std::string error_msg;
  std::unique_ptr<base::Value> metadata_ptr =
      deserializer.Deserialize(&error_code, &error_msg);
  DLOG_IF(ERROR, !metadata_ptr)
      << "JSON error " << error_code << ":" << error_msg;
  RCHECK(metadata_ptr, false);
  RCHECK(metadata_ptr->is_dict(), false);
  std::optional<base::Value::Dict> metadata =
      std::move(*metadata_ptr).TakeDict();
  RCHECK(ValidateMetadata(metadata), false);

  dumps_ = std::move(dumps);
  metadata_ = std::move(metadata);
  return true;
}

bool SynchronizedMinidumpManager::WriteFiles(
    const base::Value::List& dumps,
    const base::Value::Dict& metadata) {
  std::string lockfile;

  for (const auto& elem : dumps) {
    std::string dump_info;
    bool ret = base::JSONWriter::Write(elem, &dump_info);
    RCHECK(ret, false);
    lockfile += dump_info;
    lockfile += "\n";  // Add line seperatators
  }

  if (!WriteFile(lockfile_path_, lockfile)) {
    return false;
  }

  JSONFileValueSerializer serializer(metadata_path_);
  return serializer.Serialize(metadata);
}

bool SynchronizedMinidumpManager::InitializeFiles() {
  base::Value::Dict metadata;

  base::Value::Dict ratelimit_fields;
  ratelimit_fields.Set(kLockfileRatelimitPeriodStartKey, 0.0);
  ratelimit_fields.Set(kLockfileRatelimitPeriodDumpsKey, 0);
  metadata.Set(kLockfileRatelimitKey, std::move(ratelimit_fields));

  base::Value::List dumps;

  return WriteFiles(dumps, metadata);
}

bool SynchronizedMinidumpManager::AddEntryToLockFile(
    const DumpInfo& dump_info) {
  DCHECK_GE(lockfile_fd_, 0);
  DCHECK(dumps_);

  // Make sure dump_info is valid.
  if (!dump_info.valid()) {
    LOG(ERROR) << "Entry to be added is invalid";
    return false;
  }

  dumps_->Append(dump_info.GetAsValue());
  return true;
}

bool SynchronizedMinidumpManager::RemoveEntryFromLockFile(int index) {
  if (index < 0 || static_cast<size_t>(index) >= dumps_->size())
    return false;
  dumps_->erase(dumps_->begin() + index);
  return true;
}

void SynchronizedMinidumpManager::ReleaseLockFile() {
  // flock is associated with the fd entry in the open fd table, so closing
  // all fd's will release the lock. To be safe, we explicitly unlock.
  if (lockfile_fd_ >= 0) {
    if (dumps_ && metadata_)
      WriteFiles(*dumps_, *metadata_);

    UnlockAndCloseFile(lockfile_fd_);
    lockfile_fd_ = -1;
  }

  dumps_.reset();
  metadata_.reset();
}

std::vector<std::unique_ptr<DumpInfo>> SynchronizedMinidumpManager::GetDumps() {
  std::vector<std::unique_ptr<DumpInfo>> dumps;

  for (const auto& elem : *dumps_) {
    dumps.push_back(std::unique_ptr<DumpInfo>(new DumpInfo(&elem)));
  }

  return dumps;
}

bool SynchronizedMinidumpManager::SetCurrentDumps(
    const std::vector<std::unique_ptr<DumpInfo>>& dumps) {
  dumps_->clear();

  for (auto& dump : dumps) {
    dumps_->Append(dump->GetAsValue());
  }

  return true;
}

bool SynchronizedMinidumpManager::IncrementNumDumpsInCurrentPeriod() {
  DCHECK(metadata_);
  int last_dumps = GetRatelimitPeriodDumps(metadata_);
  RCHECK(last_dumps >= 0, false);

  return SetRatelimitPeriodDumps(metadata_, last_dumps + 1);
}

bool SynchronizedMinidumpManager::DecrementNumDumpsInCurrentPeriod() {
  DCHECK(metadata_);
  int last_dumps = GetRatelimitPeriodDumps(metadata_);
  if (last_dumps > 0) {
    return SetRatelimitPeriodDumps(metadata_, last_dumps - 1);
  }
  return true;
}

void SynchronizedMinidumpManager::ResetRateLimitPeriod() {
  SetRatelimitPeriodStart(metadata_, base::Time::Now());
  SetRatelimitPeriodDumps(metadata_, 0);
}

bool SynchronizedMinidumpManager::CanUploadDump() {
  base::Time cur_time = base::Time::Now();
  base::Time period_start = GetRatelimitPeriodStart(metadata_);
  int period_dumps_count = GetRatelimitPeriodDumps(metadata_);

  // If we're in invalid state, or we passed the period, reset the ratelimit.
  // When the device reboots, |cur_time| may be incorrectly reported to be a
  // very small number for a short period of time. So only consider
  // |period_start| invalid when |cur_time| is less if |cur_time| is not very
  // close to 0.
  if (period_dumps_count < 0 ||
      (cur_time < period_start &&
       cur_time.InSecondsFSinceUnixEpoch() > kRatelimitPeriodSeconds) ||
      (cur_time - period_start).InSeconds() >= kRatelimitPeriodSeconds) {
    ResetRateLimitPeriod();
    return true;
  }

  return period_dumps_count < kRatelimitPeriodMaxDumps;
}

bool SynchronizedMinidumpManager::HasDumps() {
  // Check if lockfile has entries.
  int64_t size = 0;
  if (base::GetFileSize(lockfile_path_, &size) && size > 0)
    return true;

  // Check if any files are in minidump directory
  base::DirReaderPosix reader(dump_path_.value().c_str());
  if (!reader.IsValid()) {
    DLOG(ERROR) << "Could not open minidump dir: " << dump_path_.value();
    return false;
  }

  while (reader.Next()) {
    if (strcmp(reader.name(), ".") == 0 || strcmp(reader.name(), "..") == 0)
      continue;

    const base::FilePath file_path = dump_path_.Append(reader.name());
    if (file_path != lockfile_path_ && file_path != metadata_path_)
      return true;
  }

  return false;
}

bool SynchronizedMinidumpManager::InitializeFileState() {
  if (!AcquireLockFile())
    return false;  // Error logged

  ReleaseLockFile();
  return true;
}

}  // namespace chromecast
