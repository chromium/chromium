// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/minidump/minidump_crashpad_info_writer.h"
#include "third_party/crashpad/crashpad/minidump/minidump_file_writer.h"
#include "third_party/crashpad/crashpad/minidump/minidump_simple_string_dictionary_writer.h"
#include "third_party/crashpad/crashpad/util/misc/metrics.h"

namespace crash_reporter {

namespace {

const std::map<std::string, std::string>& GetProcessSimpleAnnotations() {
  static std::map<std::string, std::string> annotations = []() -> auto {
    std::map<std::string, std::string> process_annotations;
    @autoreleasepool {
      NSBundle* outer_bundle = base::apple::OuterBundle();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      process_annotations["prod"] = "Chrome_iOS";
#else
      NSString* product = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:base::apple::CFToNSPtrCast(
                                                       kCFBundleNameKey)]);
      process_annotations["prod"] =
          base::SysNSStringToUTF8(product).append("_iOS");
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Empty means stable.
      const bool allow_empty_channel = true;
#else
      const bool allow_empty_channel = false;
#endif
      NSString* channel = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:@"KSChannelID"]);
      // Must be a developer build.
      if (!allow_empty_channel && (!channel || !channel.length))
        channel = @"developer";
      process_annotations["channel"] = base::SysNSStringToUTF8(channel);
      NSString* version =
          base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
              objectForInfoDictionaryKey:@"CFBundleVersion"]);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);
      process_annotations["plat"] = std::string("iOS");
      process_annotations["crashpad"] = std::string("yes");
    }  // @autoreleasepool
    return process_annotations;
  }
  ();
  return annotations;
}

}  // namespace

void ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations) {
  GetCrashpadClient().ProcessIntermediateDumps(annotations);
}

void ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  GetCrashpadClient().ProcessIntermediateDump(file, annotations);
}

bool ProcessExternalDump(
    const std::string& source,
    base::span<const uint8_t> data,
    const std::map<std::string, std::string>& override_annotations) {
  auto crashpad_info_stream =
      std::make_unique<crashpad::MinidumpCrashpadInfoWriter>();

  auto simple_string_dictionary_writer =
      std::make_unique<crashpad::MinidumpSimpleStringDictionaryWriter>();

  std::map<std::string, std::string> annotations =
      GetProcessSimpleAnnotations();
  annotations["prod"] = annotations["prod"] + "_" + source;

  for (auto& entry : override_annotations) {
    annotations[entry.first] = entry.second;
  }

  for (auto& entry : annotations) {
    auto writer =
        std::make_unique<crashpad::MinidumpSimpleStringDictionaryEntryWriter>();
    writer->SetKeyValue(entry.first, entry.second);
    simple_string_dictionary_writer->AddEntry(std::move(writer));
  }

  crashpad_info_stream->SetSimpleAnnotations(
      std::move(simple_string_dictionary_writer));

  crashpad::CrashReportDatabase* database = internal::GetCrashReportDatabase();
  if (!database) {
    return false;
  }
  std::unique_ptr<crashpad::CrashReportDatabase::NewReport> new_report;
  crashpad::CrashReportDatabase::OperationStatus database_status =
      database->PrepareNewCrashReport(&new_report);
  if (database_status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }

  crashpad::MinidumpFileWriter minidump;

  crashpad_info_stream->SetReportID(new_report->ReportID());
  crashpad::Settings* const settings = database->GetSettings();
  crashpad::UUID client_id;
  if (settings && settings->GetClientID(&client_id)) {
    crashpad_info_stream->SetClientID(client_id);
  }

  bool add_stream_result = minidump.AddStream(std::move(crashpad_info_stream));
  DCHECK(add_stream_result);

  if (data.size() > 0) {
    crashpad::FileWriter* attachment_writer = new_report->AddAttachment(source);
    attachment_writer->Write(data.data(), data.size());
  }

  if (!minidump.WriteEverything(new_report->Writer())) {
    return false;
  }

  crashpad::UUID uuid;
  database_status =
      database->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }
  return true;
}

void StartProcessingPendingReports() {
  GetCrashpadClient().StartProcessingPendingReports();
}

namespace internal {

bool PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments,
    base::FilePath* database_path) {
  DCHECK(!embedded_handler);     // This is not used on iOS.
  DCHECK(exe_path.empty());      // This is not used on iOS.
  DCHECK(initial_arguments.empty());
  DCHECK(initial_client);

  @autoreleasepool {
    CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
    crash_reporter_client->GetCrashDumpLocation(database_path);
    // Don't pass `url` to extensions since they never upload minidumps.
    std::string url = [NSBundle.mainBundle.bundlePath hasSuffix:@"appex"]
                          ? ""
                          : crash_reporter_client->GetUploadUrl();
    return GetCrashpadClient().StartCrashpadInProcessHandler(
        *database_path, url, GetProcessSimpleAnnotations(),
        crashpad::CrashpadClient::ProcessPendingReportsObservationCallback());
  }  // @autoreleasepool
}

}  // namespace internal
}  // namespace crash_reporter
