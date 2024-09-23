// Copyright 2016 The Chromium Authors
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

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/base/version.h"
#include "chromecast/crash/build_info.h"
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

typedef std::vector<std::unique_ptr<DumpInfo>> DumpList;

std::unique_ptr<PrefService> CreatePrefService() {
  base::FilePath prefs_path;
  CHECK(base::PathService::Get(chromecast::FILE_CAST_CONFIG, &prefs_path));
  DVLOG(1) << "Loading prefs from " << prefs_path.value();

  PrefRegistrySimple* registry = new PrefRegistrySimple;
  registry->RegisterBooleanPref(prefs::kOptInStats, true);
  registry->RegisterStringPref(::metrics::prefs::kMetricsClientID, "");
  registry->RegisterStringPref(kVirtualChannel, "");

  PrefServiceFactory prefServiceFactory;
  prefServiceFactory.SetUserPrefsFile(
      prefs_path, base::SingleThreadTaskRunner::GetCurrentDefault().get());
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
                                   PrefServiceGeneratorCallback callback)
    : release_channel_(sys_info->GetSystemReleaseChannel()),
      product_name_(sys_info->GetProductName()),
      device_model_(sys_info->GetDeviceModel()),
      board_name_(sys_info->GetBoardName()),
      board_revision_(sys_info->GetBoardRevision()),
      manufacturer_(sys_info->GetManufacturer()),
      system_version_(sys_info->GetSystemBuildNumber()),
      upload_location_(!server_url.empty() ? server_url
                                           : kCrashServerProduction),
      reboot_scheduled_(false),
      filestate_initialized_(false),
      uploader_(uploader),
      pref_service_generator_(std::move(callback)) {}

MinidumpUploader::MinidumpUploader(CastSysInfo* sys_info,
                                   const std::string& server_url)
    : MinidumpUploader(sys_info,
                       server_url,
                       nullptr,
                       base::BindRepeating(&CreatePrefService)) {}

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
    const std::vector<std::string>& attachments(dump.attachments());

    bool ignore_and_erase_dump = false;
    if (!opt_in_stats) {
      LOG(INFO) << "OptInStats is false, removing crash dump";
      ignore_and_erase_dump = true;
    } else if (IsDumpObsolete(dump)) {
      NOTREACHED();
    }

    // Ratelimiting persists across reboots.
    if (reboot_scheduled_) {
      LOG(INFO) << "Already rate limited with a reboot scheduled, removing "
                   "crash dump";
      ignore_and_erase_dump = true;
    } else if (CanUploadDump()) {
      // Record dump for ratelimiting
      IncrementNumDumpsInCurrentPeriod();
    } else {
      LOG(INFO) << "Can't upload dump due to rate limit, will reboot";
      ResetRateLimitPeriod();
      ignore_and_erase_dump = true;
      reboot_scheduled_ = true;
    }

    if (ignore_and_erase_dump) {
      base::DeleteFile(dump_path);
      base::DeleteFile(log_path);
      for (const auto& attachment : attachments) {
        base::FilePath attachment_path(attachment);
        if (attachment_path.DirName() == dump_path.DirName()) {
          base::DeleteFile(attachment_path);
        }
      }
      dumps.erase(dumps.begin());
      continue;
    }

    LOG(INFO) << "OptInStats is true, uploading crash dump";

    int64_t size;
    if (!dump_path.empty() && !base::GetFileSize(dump_path, &size)) {
      // either the file does not exist, or there was an error logging its
      // path, or settings its permission; regardless, we can't upload it.
      for (const auto& attachment : attachments) {
        base::FilePath attachment_path(attachment);
        if (attachment_path.DirName() == dump_path.DirName()) {
          base::DeleteFile(attachment_path);
        }
      }
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

    // attempt to upload
    LOG(INFO) << "Uploading crash to " << upload_location_;
    CastCrashdumpData crashdump_data;
    crashdump_data.product = kProductName;
    crashdump_data.version = GetVersionString(
        dump.params().cast_release_version, dump.params().cast_build_number);
    crashdump_data.guid = client_id;
    crashdump_data.ptime = uptime_stream.str();
    crashdump_data.comments = comment.str();
    crashdump_data.minidump_pathname = dump_path.value();
    crashdump_data.crash_server = upload_location_;

    // set upload_file parameter based on exec_name
    std::string upload_filename;
    if (dump.params().exec_name == "kernel") {
      upload_filename = "upload_file_ramoops";
    } else {
      upload_filename = "upload_file_minidump";
    }
    crashdump_data.upload_filename = std::move(upload_filename);

    // Depending on if a testing CastCrashdumpUploader object has been set,
    // assign |g| as a reference to the correct object.
    CastCrashdumpUploader vanilla(crashdump_data);
    CastCrashdumpUploader& g = (uploader_ ? *uploader_ : vanilla);

    if (!log_path.empty() && !g.AddAttachment("log_file", log_path.value())) {
      LOG(ERROR) << "Could not attach log file " << log_path.value();
      // Don't fail to upload just because of this.
      comment << "Could not attach log file " << log_path.value() << ". ";
    }

    int attachment_count = 0;
    for (const auto& attachment : attachments) {
      std::string label =
          "attachment_" + base::NumberToString(attachment_count++);
      g.AddAttachment(label, attachment);
    }

    // Dump some Android properties directly into product data.
    g.SetParameter("ro.revision", board_revision_);
    g.SetParameter("ro.product.release.track", release_channel_);
    g.SetParameter("ro.hardware", board_name_);
    g.SetParameter("ro.product.name", product_name_);
    g.SetParameter("device", product_name_);
    g.SetParameter("ro.product.model", device_model_);
    g.SetParameter("ro.product.manufacturer", manufacturer_);
    g.SetParameter("ro.system.version", system_version_);
    g.SetParameter("release.virtual-channel", virtual_channel);
    g.SetParameter("ro.build.type", GetBuildVariant());
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
    if (!dump.params().exec_name.empty()) {
      g.SetParameter("exec_name", dump.params().exec_name);
    }
    if (!dump.params().stadia_session_id.empty()) {
      g.SetParameter("stadia_session_id", dump.params().stadia_session_id);
    }
    if (!dump.params().signature.empty()) {
      g.SetParameter("signature", dump.params().signature);
    }
    if (!dump.params().extra_info.empty()) {
      std::vector<std::string> pairs = base::SplitString(dump.params().extra_info,
                                                         " ",
                                                         base::TRIM_WHITESPACE,
                                                         base::SPLIT_WANT_NONEMPTY
                                                        );
      for (const auto& pair : pairs) {
        std::vector<std::string> key_value =
                base::SplitString(pair, "=", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
        if (key_value.size() == 2) {
          g.SetParameter(key_value[0], key_value[1]);
        }
      }
    }

    // Set CastLite specific crash report data.
    if (!dump.params().comments.empty()) {
      g.SetParameter("comments", dump.params().comments);
    }
    if (!dump.params().js_engine.empty()) {
      g.SetParameter("js_engine", dump.params().js_engine);
    }
    if (!dump.params().js_build_label.empty()) {
      g.SetParameter("js_build_label", dump.params().js_build_label);
    }
    if (!dump.params().js_exception_category.empty()) {
      g.SetParameter("js_exception_category",
                     dump.params().js_exception_category);
    }
    if (!dump.params().js_exception_details.empty()) {
      g.SetParameter("js_exception_details",
                     dump.params().js_exception_details);
    }
    if (!dump.params().js_exception_signature.empty()) {
      // Upload as "signature" to populate the "Stable Signature" field
      g.SetParameter("signature", dump.params().js_exception_signature);
    }

    std::string response;
    if (!g.Upload(&response)) {
      // We have failed to upload this file.
      // Save our state by flushing our dumps to the lockfile
      // We'll come back around later and try again.
      LOG(ERROR) << "Upload report failed. response: " << response;
      // The increment will happen when it retries the upload.
      DecrementNumDumpsInCurrentPeriod();
      SetCurrentDumps(dumps);
      return true;
    }

    LOG(INFO) << "Uploaded report id " << response;
    // upload succeeded, so delete the entry
    dumps.erase(dumps.begin());
    // delete the dump if it exists in /data/minidumps.
    // (We may use a fake dump file which should not be deleted.)
    if (!dump_path.empty() && dump_path.DirName() == dump_path_ &&
        !base::DeleteFile(dump_path)) {
      PLOG(WARNING) << "remove dump " << dump_path.value() << " failed";
    }
    // delete the log if exists
    if (!log_path.empty() && !base::DeleteFile(log_path)) {
      PLOG(WARNING) << "remove log " << log_path.value() << " failed";
    }
    // delete the attachments
    if (!dump_path.empty()) {
      for (const auto& attachment : attachments) {
        base::FilePath attachment_path(attachment);
        if (attachment_path.DirName() == dump_path.DirName() &&
            !base::DeleteFile(attachment_path)) {
          PLOG(WARNING) << "remove attachment " << attachment << " failed";
        }
      }
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
