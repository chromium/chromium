// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_UTILS_H_
#define CHROME_CHROME_CLEANER_LOGGING_UTILS_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/chrome_cleaner/logging/info_sampler.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"

namespace chrome_cleaner {
#define LOG_PUP(severity, pup) \
  LOG(severity) << "UwS " << PUPData::GetPUPName(pup) << "(" << pup->id << ")"
// Sets |*channel| as the user's Chrome channel if defined by the
// --chrome-channel switch. Returns true if the channel can be determined.
bool GetChromeChannelFromCommandLine(int* channel);

// Retrieve the folder information path and dates into |folder_information|.
// Returns true if |folder_path| could be expanded.
bool RetrieveFolderInformation(const base::FilePath& folder_path,
                               FolderInformation* folder_information);

// Enum type for the flags used to log the detected UwS.
// |only_one_footprint| should be set to true if the scanner stopped at the
// first found active footprint
using UwSDetectedFlags = uint32_t;
enum : UwSDetectedFlags {
  kUwSDetectedFlagsNone = 0,
  kUwSDetectedFlagsOnlyOneFootprint = 1 << 0,
};

// Returns a new UwS proto with matching information in |found_uws|. For each
// |file_path|, if |cleaning_files| is set to true, detailed information is
// collected from any active and forced active files and a random sample of
// information is returned for the rest of files. If |cleaning_files| is set to
// false, gather a random sample of detailed information. |sampler| is used to
// choose the sample.
UwS PUPToUwS(const PUPData::PUP* found_uws,
             UwSDetectedFlags flags,
             bool cleaning_files,
             InfoSampler* sampler);

// Convert an internal::FileInformation struct to its corresponding
// FileInformation proto object.
void FileInformationToProtoObject(
    const internal::FileInformation& file_information,
    FileInformation* proto_file_information);

// Convert an internal::RegistryFootprint struct to its corresponding
// MatchedRegistryEntry proto object.
void MatchedRegistryEntryToProtoObject(
    const PUPData::RegistryFootprint& registry_footprint,
    MatchedRegistryEntry* entry);

// Returns a FileInformation proto object with basic information for the file
// with path |file_path|. If |include_details|, detailed information (including
// the digest) will be included. Otherwise, only basic information will be
// retrieved.
bool GetFileInformationProtoObject(const base::FilePath& file_path,
                                   bool include_details,
                                   FileInformation* file_information);

// Returns ProcessInformation message with process resource usage stats.
ProcessInformation GetProcessInformationProtoObject(
    SandboxType process_type,
    const SystemResourceUsage& usage);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_UTILS_H_
