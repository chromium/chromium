// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <CoreFoundation/CoreFoundation.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"
#include "third_party/crashpad/crashpad/minidump/minidump_file_writer.h"
#include "third_party/crashpad/crashpad/snapshot/mac/process_snapshot_mac.h"

namespace crash_reporter {

namespace {

std::map<std::string, std::string> GetProcessSimpleAnnotations() {
  static std::map<std::string, std::string> annotations = []() -> auto {
    std::map<std::string, std::string> process_annotations;
    @autoreleasepool {
      NSBundle* outer_bundle = base::apple::OuterBundle();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      process_annotations["prod"] = "Chrome_Mac";
#else
      NSString* product = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:base::apple::CFToNSPtrCast(
                                                       kCFBundleNameKey)]);
      process_annotations["prod"] =
          base::SysNSStringToUTF8(product).append("_Mac");
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Empty means stable.
      const bool allow_empty_channel = true;
#else
      const bool allow_empty_channel = false;
#endif
      NSString* channel = base::apple::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:@"KSChannelID"]);
      if (!channel || [channel isEqual:@"arm64"] ||
          [channel isEqual:@"universal"]) {
        if (allow_empty_channel)
          process_annotations["channel"] = "";
      } else {
        if ([channel hasPrefix:@"arm64-"])
          channel = [channel substringFromIndex:[@"arm64-" length]];
        else if ([channel hasPrefix:@"universal-"])
          channel = [channel substringFromIndex:[@"universal-" length]];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        if ([channel isEqual:@"extended"]) {
          // Extended stable reports as stable with an extra bool.
          channel = @"";
          process_annotations["extended_stable_channel"] = "true";
        }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        if (allow_empty_channel || [channel length]) {
          process_annotations["channel"] = base::SysNSStringToUTF8(channel);
        }
      }

      NSString* version =
          base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
              objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);

      process_annotations["plat"] = std::string("OS X");
    }  // @autoreleasepool
    return process_annotations;
  }();
  return annotations;
}

}  // namespace

void DumpProcessWithoutCrashing(task_t task_port) {
  crashpad::CrashReportDatabase* database = internal::GetCrashReportDatabase();
  if (!database)
    return;

  crashpad::ProcessSnapshotMac snapshot;
  if (!snapshot.Initialize(task_port))
    return;

  auto process_annotations = GetProcessSimpleAnnotations();
  process_annotations["is-dump-process-without-crashing"] = "true";
  snapshot.SetAnnotationsSimpleMap(process_annotations);

  std::unique_ptr<crashpad::CrashReportDatabase::NewReport> new_report;
  if (database->PrepareNewCrashReport(&new_report) !=
      crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  crashpad::UUID client_id;
  database->GetSettings()->GetClientID(&client_id);

  snapshot.SetReportID(new_report->ReportID());
  snapshot.SetClientID(client_id);

  crashpad::MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(&snapshot);
  if (!minidump.WriteEverything(new_report->Writer()))
    return;

  crashpad::UUID report_id;
  database->FinishedWritingCrashReport(std::move(new_report), &report_id);
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
  base::FilePath metrics_path;  // Only valid in the browser process.
  DCHECK(!embedded_handler);  // This is not used on Mac.
  DCHECK(exe_path.empty());   // This is not used on Mac.
  DCHECK(initial_arguments.empty());

  if (initial_client) {
    @autoreleasepool {
      base::FilePath framework_bundle_path = base::apple::FrameworkBundlePath();
      base::FilePath handler_path =
          framework_bundle_path.Append("Helpers").Append(
              "chrome_crashpad_handler");

      // Is there a way to recover if this fails?
      CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
      crash_reporter_client->GetCrashDumpLocation(database_path);
      crash_reporter_client->GetCrashMetricsLocation(&metrics_path);

      std::string url = crash_reporter_client->GetUploadUrl();

      std::vector<std::string> arguments;

      if (crash_reporter_client->ShouldMonitorCrashHandlerExpensively()) {
        arguments.push_back("--monitor-self");
      }

      // Set up --monitor-self-annotation even in the absence of --monitor-self
      // so that minidumps produced by Crashpad's generate_dump tool will
      // contain these annotations.
      arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

      if (!browser_process) {
        // If this is an initial client that's not the browser process, it's
        // important that the new Crashpad handler also not be connected to any
        // existing handler. This argument tells the new Crashpad handler to
        // sever this connection.
        arguments.push_back(
            "--reset-own-crash-exception-port-to-system-default");
      }

      bool result = GetCrashpadClient().StartHandler(
          handler_path, *database_path, metrics_path, url,
          GetProcessSimpleAnnotations(), arguments, true, false);

      // If this is an initial client that's not the browser process, it's
      // important to sever the connection to any existing handler. If
      // StartHandler() failed, call UseSystemDefaultHandler() to drop the link
      // to the existing handler.
      if (!result && !browser_process) {
        crashpad::CrashpadClient::UseSystemDefaultHandler();
        return false;
      }
    }  // @autoreleasepool
  }

  return true;
}

}  // namespace internal
}  // namespace crash_reporter
