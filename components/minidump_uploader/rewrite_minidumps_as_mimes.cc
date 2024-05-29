// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/minidump_uploader/rewrite_minidumps_as_mimes.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/crash/android/anr_build_id_provider.h"
#include "components/crash/android/anr_skipped_reason.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "third_party/crashpad/crashpad/handler/minidump_to_upload_parameters.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_writer.h"
#include "third_party/crashpad/crashpad/util/net/http_body.h"
#include "third_party/crashpad/crashpad/util/net/http_multipart_builder.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/minidump_uploader/minidump_uploader_jni_headers/CrashReportMimeWriter_jni.h"

namespace minidump_uploader {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProcessedMinidumpCounts {
  kOther = 0,
  kBrowser = 1,
  kRenderer = 2,
  kGpu = 3,
  kUtility = 4,
  kMaxValue = kUtility
};

bool MimeifyReportWithKeyValuePairs(
    const crashpad::CrashReportDatabase::UploadReport& report,
    crashpad::HTTPMultipartBuilder* http_multipart_builder,
    std::vector<std::string>* crashes_key_value_arr,
    pid_t* pid) {
  crashpad::FileReader* reader = report.Reader();
  crashpad::FileOffset start_offset = reader->SeekGet();
  if (start_offset < 0) {
    return false;
  }

  // Ignore any errors that might occur when attempting to interpret the
  // minidump file. This may result in its being uploaded with few or no
  // parameters, but as long as there’s a dump file, the server can decide what
  // to do with it.
  std::map<std::string, std::string> parameters;
  crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
  if (minidump_process_snapshot.Initialize(reader)) {
    parameters =
        BreakpadHTTPFormParametersFromMinidump(&minidump_process_snapshot);
  }

  if (!reader->SeekSet(start_offset)) {
    return false;
  }

  static constexpr char kMinidumpKey[] = "upload_file_minidump";
  static constexpr char kPtypeKey[] = "ptype";

  for (const auto& kv : parameters) {
    if (kv.first == kMinidumpKey) {
      LOG(WARNING) << "reserved key " << kv.first << ", discarding value "
                   << kv.second;
    } else {
      http_multipart_builder->SetFormData(kv.first, kv.second);
      if (crashes_key_value_arr) {
        crashes_key_value_arr->push_back(kv.first);
        crashes_key_value_arr->push_back(kv.second);
      }
      if (kv.first == kPtypeKey) {
        const crashpad::ExceptionSnapshot* exception =
            minidump_process_snapshot.Exception();
        if (exception != nullptr) {
          const uint32_t signo = exception->Exception();
          ProcessedMinidumpCounts count_type;
          if (kv.second == "browser") {
            count_type = ProcessedMinidumpCounts::kBrowser;
          } else if (kv.second == "renderer") {
            count_type = ProcessedMinidumpCounts::kRenderer;
          } else if (kv.second == "gpu-process") {
            count_type = ProcessedMinidumpCounts::kGpu;
          } else if (kv.second == "utility") {
            count_type = ProcessedMinidumpCounts::kUtility;
          } else {
            count_type = ProcessedMinidumpCounts::kOther;
          }
          if (signo !=
              static_cast<uint32_t>(crashpad::Signals::kSimulatedSigno)) {
            UMA_HISTOGRAM_ENUMERATION(
                "Stability.Android.ProcessedRealMinidumps", count_type);
          }
        }
      }
    }
  }

  if (crashes_key_value_arr) {
    crashes_key_value_arr->push_back(kMinidumpKey);
    crashes_key_value_arr->push_back(report.uuid.ToString().c_str());
  }

  http_multipart_builder->SetFileAttachment(kMinidumpKey,
                                            report.uuid.ToString() + ".dmp",
                                            reader, "application/octet-stream");

  *pid = minidump_process_snapshot.ProcessID();
  return true;
}

bool MimeifyReportAndWriteToDirectory(
    const crashpad::CrashReportDatabase::UploadReport& report,
    const base::FilePath& dest_dir,
    std::vector<std::string>* crashes_key_value_arr) {
  crashpad::HTTPMultipartBuilder builder;
  pid_t pid;
  if (!MimeifyReportWithKeyValuePairs(report, &builder, crashes_key_value_arr,
                                      &pid)) {
    return false;
  }

  crashpad::FileWriter writer;
  if (!writer.Open(dest_dir.Append(base::StringPrintf(
                       "%s.dmp%d", report.uuid.ToString().c_str(), pid)),
                   crashpad::FileWriteMode::kCreateOrFail,
                   crashpad::FilePermissions::kOwnerOnly)) {
    return false;
  }

  return WriteBodyToFile(builder.GetBodyStream().get(), &writer);
}

}  // namespace

bool MimeifyReport(const crashpad::CrashReportDatabase::UploadReport& report,
                   crashpad::HTTPMultipartBuilder* http_multipart_builder,
                   pid_t* pid) {
  return MimeifyReportWithKeyValuePairs(report, http_multipart_builder, nullptr,
                                        pid);
}

bool WriteBodyToFile(crashpad::HTTPBodyStream* body,
                     crashpad::FileWriterInterface* writer) {
  uint8_t buffer[4096];
  crashpad::FileOperationResult bytes_read;
  while ((bytes_read = body->GetBytesBuffer(buffer, sizeof(buffer))) > 0) {
    writer->Write(buffer, bytes_read);
  }
  return bytes_read == 0;
}

void RewriteMinidumpsAsMIMEs(const base::FilePath& src_dir,
                             const base::FilePath& dest_dir,
                             std::vector<std::string>* crashes_key_value_arr) {
  std::unique_ptr<crashpad::CrashReportDatabase> db =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(src_dir);
  if (!db) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  if (db->GetPendingReports(&reports) !=
      crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  for (const auto& report : reports) {
    std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
        upload_report;
    switch (db->GetReportForUploading(report.uuid,
                                      &upload_report,
                                      /* report_metrics= */ false)) {
      case crashpad::CrashReportDatabase::kBusyError:
      case crashpad::CrashReportDatabase::kReportNotFound:
        continue;

      case crashpad::CrashReportDatabase::kNoError:
        if (MimeifyReportAndWriteToDirectory(*upload_report.get(), dest_dir,
                                             crashes_key_value_arr)) {
          db->RecordUploadComplete(std::move(upload_report), std::string());
        } else {
          crashpad::Metrics::CrashUploadSkipped(
              crashpad::Metrics::CrashSkippedReason::kPrepareForUploadFailed);
          upload_report.reset();
        }
        db->DeleteReport(report.uuid);
        continue;

      case crashpad::CrashReportDatabase::kFileSystemError:
      case crashpad::CrashReportDatabase::kDatabaseError:
        crashpad::Metrics::CrashUploadSkipped(
            crashpad::Metrics::CrashSkippedReason::kDatabaseError);
        db->DeleteReport(report.uuid);
        continue;

      case crashpad::CrashReportDatabase::kCannotRequestUpload:
        NOTREACHED_IN_MIGRATION();
        db->DeleteReport(report.uuid);
        continue;
    }
  }
}

static void reportAnrUploadFailure(AnrSkippedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Crashpad.AnrUpload.Skipped", reason);
}

void WriteAnrAsMime(crashpad::FileReader* anr_reader,
                    crashpad::FileWriterInterface* writer,
                    const std::string& version_number,
                    const std::string& build_id,
                    const std::string& anr_file_name) {
  crashpad::HTTPMultipartBuilder builder;
  builder.SetFormData("version", version_number);
  builder.SetFormData("product", "Chrome_Android");
  std::string channel = std::string(
      version_info::GetChannelString(version_info::android::GetChannel()));
  if (channel == "stable") {
    // Android reports require an empty string instead of "stable".
    channel = "";
  }
  builder.SetFormData("channel", channel);

  if (build_id.empty()) {
    if (version_number == version_info::GetVersionNumber()) {
      // We have an ANR where we didn't pre-set the build ID in the process
      // state summary, but since we are currently on the same version we can
      // just use our current one.
      builder.SetFormData("elf_build_id", crash_reporter::GetElfBuildId());
    }
  } else {
    builder.SetFormData("elf_build_id", build_id);
  }

  // We can't use crashpad::AnnotationList::Get() as it contains a number of
  // fields which change on each Chrome restart.
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  builder.SetFormData("android_build_id", info->android_build_id());
  builder.SetFormData("android_build_fp", info->android_build_fp());
  builder.SetFormData("sdk", base::StringPrintf("%d", info->sdk_int()));
  builder.SetFormData("device", info->device());
  builder.SetFormData("model", info->model());
  builder.SetFormData("brand", info->brand());
  builder.SetFormData("board", info->board());
  builder.SetFormData("installer_package_name", info->installer_package_name());
  builder.SetFormData("abi_name", info->abi_name());
  builder.SetFormData("custom_themes", info->custom_themes());
  builder.SetFormData("resources_version", info->resources_version());
  builder.SetFormData("gms_core_version", info->gms_version_code());

  // The package name and version are used for deobfuscation, but will
  // only be accurate for the same version of chrome.
  if (version_number == version_info::GetVersionNumber()) {
    builder.SetFormData("package", std::string(info->package_name()) + " v" +
                                       info->package_version_code() + " (" +
                                       info->package_version_name() + ")");
  }

  if (anr_reader != nullptr) {
    builder.SetFileAttachment("anr_data", anr_file_name, anr_reader,
                              "application/octet-stream");
  }
  if (!WriteBodyToFile(builder.GetBodyStream().get(), writer)) {
    reportAnrUploadFailure(AnrSkippedReason::kFilesystemWriteFailure);
  }
}

static void JNI_CrashReportMimeWriter_RewriteAnrsAsMIMEs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_anrs,
    const base::android::JavaParamRef<jstring>& j_dest_dir) {
  std::vector<std::string> anr_strings;
  base::android::AppendJavaStringArrayToStringVector(env, j_anrs, &anr_strings);
  std::string dest_dir;
  base::android::ConvertJavaStringToUTF8(env, j_dest_dir, &dest_dir);

  for (size_t i = 0; i < anr_strings.size(); i += 3) {
    std::string anr_proto_file_path = anr_strings.at(i);
    std::string chrome_version = anr_strings.at(i + 1);
    std::string build_id = anr_strings.at(i + 2);
    crashpad::FileWriter writer;
    crashpad::FileReader reader;
    crashpad::UUID uuid;
    uuid.InitializeWithNew();
    std::string anr_file_name = uuid.ToString() + "_ANR.dmp";
    if (!reader.Open(base::FilePath(anr_proto_file_path))) {
      reportAnrUploadFailure(AnrSkippedReason::kFilesystemReadFailure);
      continue;
    }
    if (!writer.Open(base::FilePath(dest_dir).Append(anr_file_name),
                     crashpad::FileWriteMode::kCreateOrFail,
                     crashpad::FilePermissions::kOwnerOnly)) {
      reportAnrUploadFailure(AnrSkippedReason::kFilesystemWriteFailure);
      continue;
    }

    WriteAnrAsMime(&reader, &writer, chrome_version, build_id, anr_file_name);
  }
}

static void JNI_CrashReportMimeWriter_RewriteMinidumpsAsMIMEs(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_src_dir,
    const base::android::JavaParamRef<jstring>& j_dest_dir) {
  std::string src_dir, dest_dir;
  base::android::ConvertJavaStringToUTF8(env, j_src_dir, &src_dir);
  base::android::ConvertJavaStringToUTF8(env, j_dest_dir, &dest_dir);

  RewriteMinidumpsAsMIMEs(base::FilePath(src_dir), base::FilePath(dest_dir),
                          nullptr);
}

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_CrashReportMimeWriter_RewriteMinidumpsAsMIMEsAndGetCrashKeys(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_src_dir,
    const base::android::JavaParamRef<jstring>& j_dest_dir) {
  std::string src_dir, dest_dir;
  base::android::ConvertJavaStringToUTF8(env, j_src_dir, &src_dir);
  base::android::ConvertJavaStringToUTF8(env, j_dest_dir, &dest_dir);

  std::vector<std::string> key_value_arr;
  RewriteMinidumpsAsMIMEs(base::FilePath(src_dir), base::FilePath(dest_dir),
                          &key_value_arr);

  return base::android::ToJavaArrayOfStrings(env, key_value_arr);
}

}  // namespace minidump_uploader
