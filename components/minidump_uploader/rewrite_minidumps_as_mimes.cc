// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/minidump_uploader/rewrite_minidumps_as_mimes.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/minidump_uploader/minidump_uploader_jni_headers/CrashReportMimeWriter_jni.h"
#include "third_party/crashpad/crashpad/handler/minidump_to_upload_parameters.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_writer.h"
#include "third_party/crashpad/crashpad/util/net/http_body.h"
#include "third_party/crashpad/crashpad/util/net/http_multipart_builder.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

namespace minidump_uploader {

namespace {

#if defined(OS_ANDROID)
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
#endif  // OS_ANDROID

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
#if defined(OS_ANDROID)
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
          } else {
            UMA_HISTOGRAM_ENUMERATION(
                "Stability.Android.ProcessedSimulatedMinidumps", count_type);
          }
        }
        // TODO(wnwen): Add histogram for number of null exceptions.
      }
#endif  // OS_ANDROID
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
        NOTREACHED();
        db->DeleteReport(report.uuid);
        continue;
    }
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
