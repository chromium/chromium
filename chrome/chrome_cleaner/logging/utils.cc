// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/utils.h"

#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/settings/engine_settings.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

bool GetChromeChannelFromCommandLine(int* channel) {
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kChromeChannelSwitch))
    return false;
  std::string channel_string =
      command_line->GetSwitchValueASCII(kChromeChannelSwitch);
  return base::StringToInt(channel_string, channel) && *channel >= 0;
}

bool RetrieveFolderInformation(const base::FilePath& folder_path,
                               FolderInformation* folder_information) {
  base::FilePath expanded_path;
  if (!TryToExpandPath(folder_path, &expanded_path) || expanded_path.empty() ||
      !base::DirectoryExists(expanded_path)) {
    return false;
  }

  internal::FileInformation file_information;
  RetrievePathInformation(expanded_path, &file_information);

  folder_information->set_path(base::UTF16ToUTF8(file_information.path));
  folder_information->set_creation_date(file_information.creation_date);
  folder_information->set_last_modified_date(
      file_information.last_modified_date);
  return true;
}

void LogFileInformation(const base::FilePath& file_path,
                        bool log_detailed_info,
                        bool is_active,
                        UwS* detected_uws) {
  internal::FileInformation file_information;
  if (!RetrieveFileInformation(file_path, log_detailed_info,
                               &file_information)) {
    return;
  }

  file_information.active_file = is_active;
  MatchedFile* matched_file = detected_uws->add_files();
  FileInformationToProtoObject(file_information,
                               matched_file->mutable_file_information());
  matched_file->set_removal_status(REMOVAL_STATUS_MATCHED_ONLY);
  matched_file->set_quarantine_status(QUARANTINE_STATUS_UNSPECIFIED);
}

UwS PUPToUwS(const PUPData::PUP* found_uws,
             UwSDetectedFlags flags,
             bool cleaning_files,
             InfoSampler* sampler) {
  DCHECK(found_uws);
  const PUPData::UwSSignature& found_signature = found_uws->signature();

  UwS detected_uws;
  detected_uws.set_id(found_signature.id);
  if (found_signature.name != nullptr)
    detected_uws.set_name(found_signature.name);
  UwS::DetailLevel* detail_level = detected_uws.mutable_detail_level();
  bool only_one_footprint = (flags & kUwSDetectedFlagsOnlyOneFootprint) != 0;
  detail_level->set_only_one_footprint(only_one_footprint);

  if (PUPData::HasRemovalFlag(found_signature.flags)) {
    detected_uws.set_state(UwS::REMOVABLE);
  } else {
    detected_uws.set_state(UwS::REPORT_ONLY);
  }
  detected_uws.set_detected_by(PUPData::GetEngine(found_signature.id));

  // Get information on each folder, and split the files into active and
  // inactive lists.
  FilePathSet active_files;
  FilePathSet inactive_files;
  for (const auto& disk_footprint :
       found_uws->expanded_disk_footprints.file_paths()) {
    if (base::DirectoryExists(disk_footprint)) {
      FolderInformation folder_information;
      if (RetrieveFolderInformation(disk_footprint, &folder_information)) {
        MatchedFolder* matched_folder = detected_uws.add_folders();
        *matched_folder->mutable_folder_information() = folder_information;
        matched_folder->set_removal_status(REMOVAL_STATUS_MATCHED_ONLY);
      }
    } else if (PathHasActiveExtension(disk_footprint)) {
      active_files.Insert(disk_footprint);
    } else {
      inactive_files.Insert(disk_footprint);
    }
  }

  // Both cleaning and reporting mode will log details of a sample of
  // inactive files.
  FilePathSet collected_files = inactive_files;

  if (cleaning_files) {
    // Log details for all active files.
    for (const auto& disk_footprint : active_files.file_paths()) {
      LogFileInformation(disk_footprint,
                         /*log_detailed_info=*/true, /*is_active=*/true,
                         &detected_uws);
    }
  } else {
    // Add the active files to the set of files to sample.
    collected_files.CopyFrom(active_files);
  }

  // Select a subset of files that haven't been logged yet for detailed
  // information sampling. Only log basic information for the rest.
  FilePathSet files_to_sample;
  DCHECK(sampler);
  sampler->SelectPathSetToSample(collected_files, &files_to_sample);
  for (const auto& file_path : collected_files.ToVector()) {
    LogFileInformation(file_path, files_to_sample.Contains(file_path),
                       active_files.Contains(file_path), &detected_uws);
  }

  // Collect information on registry footprints.
  for (const auto& registry_footprint :
       found_uws->expanded_registry_footprints) {
    MatchedRegistryEntry* entry = detected_uws.add_registry_entries();
    MatchedRegistryEntryToProtoObject(registry_footprint, entry);
  }

  // Collect information on scheduled tasks.
  for (const auto& expanded_scheduled_task :
       found_uws->expanded_scheduled_tasks) {
    ScheduledTask* reported_task =
        detected_uws.add_scheduled_tasks()->mutable_scheduled_task();
    reported_task->set_name(base::UTF16ToUTF8(expanded_scheduled_task));
  }

  // Collect trace locations.
  std::set<UwS::TraceLocation> trace_locations;
  for (const auto& path_info_it : found_uws->disk_footprints_info.map()) {
    trace_locations.insert(path_info_it.second.found_in.begin(),
                           path_info_it.second.found_in.end());
  }
  for (UwS::TraceLocation location : trace_locations)
    detected_uws.add_trace_locations(location);

  return detected_uws;
}

void FileInformationToProtoObject(
    const internal::FileInformation& file_information,
    FileInformation* proto_file_information) {
  DCHECK(proto_file_information);
  if (file_information.path.empty())
    return;
  proto_file_information->set_path(base::UTF16ToUTF8(file_information.path));
  if (!file_information.creation_date.empty())
    proto_file_information->set_creation_date(file_information.creation_date);
  if (!file_information.last_modified_date.empty()) {
    proto_file_information->set_last_modified_date(
        file_information.last_modified_date);
  }
  if (!file_information.sha256.empty())
    proto_file_information->set_sha256(file_information.sha256);
  proto_file_information->set_size(file_information.size);
  if (!file_information.company_name.empty()) {
    proto_file_information->set_company_name(
        base::UTF16ToUTF8(file_information.company_name));
  }
  if (!file_information.company_short_name.empty()) {
    proto_file_information->set_company_short_name(
        base::UTF16ToUTF8(file_information.company_short_name));
  }
  if (!file_information.product_name.empty()) {
    proto_file_information->set_product_name(
        base::UTF16ToUTF8(file_information.product_name));
  }
  if (!file_information.product_short_name.empty()) {
    proto_file_information->set_product_short_name(
        base::UTF16ToUTF8(file_information.product_short_name));
  }
  if (!file_information.internal_name.empty()) {
    proto_file_information->set_internal_name(
        base::UTF16ToUTF8(file_information.internal_name));
  }
  if (!file_information.original_filename.empty()) {
    proto_file_information->set_original_filename(
        base::UTF16ToUTF8(file_information.original_filename));
  }
  if (!file_information.file_description.empty()) {
    proto_file_information->set_file_description(
        base::UTF16ToUTF8(file_information.file_description));
  }
  if (!file_information.file_version.empty()) {
    proto_file_information->set_file_version(
        base::UTF16ToUTF8(file_information.file_version));
  }
  proto_file_information->set_active_file(file_information.active_file);
}

void MatchedRegistryEntryToProtoObject(
    const PUPData::RegistryFootprint& registry_footprint,
    MatchedRegistryEntry* entry) {
  DCHECK(entry);
  auto full_key_path = registry_footprint.key_path.FullPath();
  if (full_key_path.empty())
    return;
  entry->set_key_path(base::WideToUTF8(full_key_path));
  if (!registry_footprint.value_name.empty())
    entry->set_value_name(base::WideToUTF8(registry_footprint.value_name));
  if (!registry_footprint.value_substring.empty()) {
    entry->set_value_substring(
        base::WideToUTF8(registry_footprint.value_substring));
  }
}

bool GetFileInformationProtoObject(const base::FilePath& file_path,
                                   bool include_details,
                                   FileInformation* file_information) {
  internal::FileInformation internal_file_information;
  if (!RetrieveFileInformation(file_path, include_details,
                               &internal_file_information))
    return false;

  FileInformationToProtoObject(internal_file_information, file_information);
  return true;
}

ProcessInformation GetProcessInformationProtoObject(
    SandboxType process_type,
    const SystemResourceUsage& usage) {
  ProcessInformation process_info;
  switch (process_type) {
    case SandboxType::kNonSandboxed:
      process_info.set_process(ProcessInformation::MAIN);
      break;
    case SandboxType::kEngine:
      process_info.set_process(
          GetEngineProcessType(Settings::GetInstance()->engine()));
      break;
    case SandboxType::kParser:
      process_info.set_process(ProcessInformation::PARSER_SANDBOX);
      break;
    case SandboxType::kZipArchiver:
      process_info.set_process(ProcessInformation::ZIP_ARCHIVER_SANDBOX);
      break;
    default:
      NOTREACHED() << "Unknown sandbox type " << static_cast<int>(process_type);
  }

  ProcessInformation::SystemResourceUsage* usage_msg =
      process_info.mutable_resource_usage();
  usage_msg->set_user_time(usage.user_time.InSeconds());
  usage_msg->set_kernel_time(usage.kernel_time.InSeconds());
  usage_msg->set_peak_working_set_size(usage.peak_working_set_size);
  usage_msg->set_read_operation_count(usage.io_counters.ReadOperationCount);
  usage_msg->set_write_operation_count(usage.io_counters.WriteOperationCount);
  usage_msg->set_other_operation_count(usage.io_counters.OtherOperationCount);
  usage_msg->set_read_transfer_count(usage.io_counters.ReadTransferCount);
  usage_msg->set_write_transfer_count(usage.io_counters.WriteTransferCount);
  usage_msg->set_other_transfer_count(usage.io_counters.OtherTransferCount);

  return process_info;
}

}  // namespace chrome_cleaner
