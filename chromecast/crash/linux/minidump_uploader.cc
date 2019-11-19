// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_uploader.h"

#include <errno.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <list>
#include <memory>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/base/version.h"
#include "chromecast/crash/cast_crashdump_uploader.h"
#include "chromecast/crash/linux/dump_info.h"
#include "chromecast/public/cast_sys_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"

namespace chromecast {

namespace {

const char kProductName[] = "Eureka";

const char kCrashServerProduction[] = "https://clients2.google.com/cr/report";

const char kVirtualChannel[] = "virtual-channel";

const char kLatestUiVersion[] = "latest-ui-version";

typedef std::vector<std::unique_ptr<DumpInfo>> DumpList;

std::unique_ptr<PrefService> CreatePrefService() {
  base::FilePath prefs_path;
  CHECK(base::PathService::Get(chromecast::FILE_CAST_CONFIG, &prefs_path));
  DVLOG(1) << "Loading prefs from " << prefs_path.value();

  PrefRegistrySimple* registry = new PrefRegistrySimple;
  registry->RegisterBooleanPref(prefs::kOptInStats, true);
  registry->RegisterStringPref(::metrics::prefs::kMetricsClientID, "");
  registry->RegisterStringPref(kVirtualChannel, "");
  registry->RegisterForeignPref(kLatestUiVersion);

  PrefServiceFactory prefServiceFactory;
  prefServiceFactory.SetUserPrefsFile(
      prefs_path, base::ThreadTaskRunnerHandle::Get().get());
  return prefServiceFactory.Create(registry);
}

bool IsDumpObsolete(const DumpInfo& dump) {
  return dump.params().cast_release_version.empty() ||
         dump.params().cast_build_number.empty();
}

}  // namespace

MinidumpUploader::MinidumpUploader(CastSysInfo* sys_info,
                                   const std::string& server_url,
                                   CastCrashdumpUploader* const uploader,
                                   const PrefServiceGeneratorCallback callback)
    : release_channel_(sys_info->GetSystemReleaseChannel()),
      product_name_(sys_info->GetProductName()),
      device_model_(sys_info->GetDeviceModel()),
      board_name_(sys_info->GetBoardName()),
      board_revision_(sys_info->GetBoardRevision()),
      manufacturer_(sys_info->GetManufacturer()),
      system_version_(sys_info->GetSystemBuildNumber()),
      upload_location_(!server_url.empty() ? server_url
                                           : kCrashServerProduction),
      last_upload_ratelimited_(true),
      reboot_scheduled_(false),
      filestate_initialized_(false),
      uploader_(uploader),
      pref_service_generator_(callback) {}

MinidumpUploader::MinidumpUploader(CastSysInfo* sys_info,
                                   const std::string& server_url)
    : MinidumpUploader(sys_info,
                       server_url,
                       nullptr,
                       base::Bind(&CreatePrefService)) {}

MinidumpUploader::~MinidumpUploader() {}

bool MinidumpUploader::UploadAllMinidumps() {
  // Create the lockfile if it doesn't exist.
  if (!filestate_initialized_)
    filestate_initialized_ = InitializeFileState();

  if (HasDumps())
    return AcquireLockAndDoWork();

  return true;
}

bool MinidumpUploader::DoWork() {
  // Read the file stream line by line into a list. As each file is uploaded,
  // it is subsequently deleted from the list. If the file cannot be found
  // (which is a possible scenario if the file uploaded previously, but the
  // device powered off before the log could be updated), it is also deleted.
  // Whenever an upload fails (due to lost connection), the remaining entries
  // on the list will overwrite the log file. This way, the log file reflects
  // the state of the un-uploaded dumps as best as it can.
  // Note: it is also possible that a dump previously uploaded exists in the
  // list, *and* can also be found. This might happen if the device powered
  // off before the dump can be deleted and the log updated. This is
  // unpreventable.

  DumpList dumps(GetDumps());

  int num_uploaded = 0;

  std::unique_ptr<PrefService> pref_service = pref_service_generator_.Run();
  const std::string& client_id(
      pref_service->GetString(::metrics::prefs::kMetricsClientID));
  std::string virtual_channel(pref_service->GetString(kVirtualChannel));
  if (virtual_channel.empty()) {
    virtual_channel = release_channel_;
  }
  bool opt_in_stats = pref_service->GetBoolean(prefs::kOptInStats);
  // Handle each dump and consume it out of the structure.
  while (dumps.size()) {
    const DumpInfo& dump = *(dumps.front());
    const base::FilePath dump_path(dump.crashed_process_dump());
    base::FilePath log_path(dump.logfile());

    bool ignore_and_erase_dump = false;
    if (!opt_in_stats) {
      LOG(INFO) << "OptInStats is false, removing crash dump";
      ignore_and_erase_dump = true;
    } else if (IsDumpObsolete(dump)) {
      NOTREACHED();
      LOG(INFO) << "DumpInfo belongs to older version, removing crash dump";
      ignore_and_erase_dump = true;
    }

    // Ratelimiting persists across reboots, thus we to keep track of
    // last_upload_ratelimited_ to detect when we first become ratelimited.
    // Otherwise once ratelimited, we will reboot every time we try to upload a
    // dump.
    if (CanUploadDump()) {
      last_upload_ratelimited_ = false;
    } else {
      LOG(INFO) << "Can't upload dump: Ratelimited.";
      ignore_and_erase_dump = true;

      // If the last upload wasn't ratelimited and this one is, then this is the
      // first time we reached the ratelimit. Reboot the device.
      if (!last_upload_ratelimited_)
        reboot_scheduled_ = true;

      last_upload_ratelimited_ = true;
    }

    // Record dump for ratelimiting
    IncrementNumDumpsInCurrentPeriod();

    if (ignore_and_erase_dump) {
      base::DeleteFile(dump_path, false);
      base::DeleteFile(log_path, false);
      dumps.erase(dumps.begin());
      continue;
    }

    LOG(INFO) << "OptInStats is true, uploading crash dump";

    int64_t size;
    if (!dump_path.empty() && !base::GetFileSize(dump_path, &size)) {
      // either the file does not exist, or there was an error logging its
      // path, or settings its permission; regardless, we can't upload it.
      dumps.erase(dumps.begin());
      continue;
    }

    std::stringstream comment;
    if (log_path.empty()) {
      comment << "Log file not specified. ";
    } else if (!base::GetFileSize(log_path, &size)) {
      comment << "Can't get size of " << log_path.value() << ": "
              << strerror(errno);
      // if we can't find the log file, don't upload the log
      log_path.clear();
    } else {
      comment << "Log size is " << size << ". ";
    }

    std::stringstream uptime_stream;
    uptime_stream << dump.params().process_uptime;

    const std::string version(dump.params().cast_release_version + "." +
                              dump.params().cast_build_number +
                              dump.params().suffix);
    // attempt to upload
    LOG(INFO) << "Uploading crash to " << upload_location_;
    CastCrashdumpData crashdump_data;
    crashdump_data.product = kProductName;
    crashdump_data.version = version;
    crashdump_data.guid = client_id;
    crashdump_data.ptime = uptime_stream.str();
    crashdump_data.comments = comment.str();
    crashdump_data.minidump_pathname = dump_path.value();
    crashdump_data.crash_server = upload_location_;

    // Depending on if a testing CastCrashdumpUploader object has been set,
    // assign |g| as a reference to the correct object.
    CastCrashdumpUploader vanilla(crashdump_data);
    CastCrashdumpUploader& g = (uploader_ ? *uploader_ : vanilla);

    if (!log_path.empty() && !g.AddAttachment("log_file", log_path.value())) {
      LOG(ERROR) << "Could not attach log file " << log_path.value();
      // Don't fail to upload just because of this.
      comment << "Could not attach log file " << log_path.value() << ". ";
    }

    // Dump some Android properties directly into product data.
    g.SetParameter("ro.revision", board_revision_);
    g.SetParameter("ro.product.release.track", release_channel_);
    g.SetParameter("ro.hardware", board_name_);
    g.SetParameter("ro.product.name", product_name_);
    g.SetParameter("ro.product.model", device_model_);
    g.SetParameter("ro.product.manufacturer", manufacturer_);
    g.SetParameter("ro.system.version", system_version_);
    g.SetParameter("release.virtual-channel", virtual_channel);
    if (pref_service->HasPrefPath(kLatestUiVersion)) {
      g.SetParameter("ui.version",
                     pref_service->GetString(kLatestUiVersion));
    }
    // Add app state information
    if (!dump.params().previous_app_name.empty()) {
      g.SetParameter("previous_app", dump.params().previous_app_name);
    }
    if (!dump.params().current_app_name.empty()) {
      g.SetParameter("current_app", dump.params().current_app_name);
    }
    if (!dump.params().last_app_name.empty()) {
      g.SetParameter("last_app", dump.params().last_app_name);
    }
    if (!dump.params().reason.empty()) {
      g.SetParameter("reason", dump.params().reason);
    }

    std::string response;
    if (!g.Upload(&response)) {
      // We have failed to upload this file.
      // Save our state by flushing our dumps to the lockfile
      // We'll come back around later and try again.
      LOG(ERROR) << "Upload report failed. response: " << response;
      SetCurrentDumps(dumps);
      return true;
    }

    LOG(INFO) << "Uploaded report id " << response;
    // upload succeeded, so delete the entry
    dumps.erase(dumps.begin());
    // delete the dump if it exists in /data/minidumps.
    // (We may use a fake dump file which should not be deleted.)
    if (!dump_path.empty() && dump_path.DirName() == dump_path_ &&
        !base::DeleteFile(dump_path, false)) {
      LOG(WARNING) << "remove dump " << dump_path.value() << " failed"
                   << strerror(errno);
    }
    // delete the log if exists
    if (!log_path.empty() && !base::DeleteFile(log_path, false)) {
      LOG(WARNING) << "remove log " << log_path.value() << " failed"
                   << strerror(errno);
    }
    ++num_uploaded;
  }

  // This will simply empty the log file.
  // Entries should either be skipped/deleted or processed/deleted.
  SetCurrentDumps(dumps);

  // If we reach here, then the log file should be empty, and there should
  // be no more dumps to upload. However, it is possible that there are
  // lingering files (for example, if the dump was written, but the log
  // updating failed). Since we have no entries on these files, we cannot
  // upload them. Therefore we should delete them. This is also a good way
  // to make sure system resources aren't being drained.

  int num_deleted = GetNumDumps(true /* delete_all_dumps */);
  if (num_deleted > 0) {
    LOG(WARNING) << num_deleted << " lingering dump files deleted.";
  }

  LOG(INFO) << num_uploaded << " dumps were uploaded.";
  return true;
}

}  // namespace chromecast
