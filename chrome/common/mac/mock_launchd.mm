// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/mock_launchd.h"

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <stddef.h>
#include <sys/un.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/service_process_util.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
bool MockLaunchd::MakeABundle(const base::FilePath& dst,
                              const std::string& name,
                              base::FilePath* bundle_root,
                              base::FilePath* executable) {
  *bundle_root = dst.Append(name + std::string(".app"));
  base::FilePath contents = bundle_root->AppendASCII("Contents");
  base::FilePath mac_os = contents.AppendASCII("MacOS");
  *executable = mac_os.Append(name);
  base::FilePath info_plist = contents.Append("Info.plist");

  if (!base::CreateDirectory(mac_os)) {
    return false;
  }
  const char* data = "#! testbundle\n";
  int len = strlen(data);
  if (base::WriteFile(*executable, data, len) != len) {
    return false;
  }
  if (chmod(executable->value().c_str(), 0555) != 0) {
    return false;
  }

  const char info_plist_format[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>CFBundleDevelopmentRegion</key>\n"
      "  <string>English</string>\n"
      "  <key>CFBundleExecutable</key>\n"
      "  <string>%s</string>\n"
      "  <key>CFBundleIdentifier</key>\n"
      "  <string>com.test.%s</string>\n"
      "  <key>CFBundleInfoDictionaryVersion</key>\n"
      "  <string>6.0</string>\n"
      "  <key>CFBundleShortVersionString</key>\n"
      "  <string>%s</string>\n"
      "  <key>CFBundleVersion</key>\n"
      "  <string>1</string>\n"
      "</dict>\n"
      "</plist>\n";
  std::string info_plist_data =
      base::StringPrintf(info_plist_format, name.c_str(), name.c_str(),
                         version_info::GetVersionNumber().c_str());
  len = info_plist_data.length();
  if (base::WriteFile(info_plist, info_plist_data.c_str(), len) != len) {
    return false;
  }
  const UInt8* bundle_root_path =
      reinterpret_cast<const UInt8*>(bundle_root->value().c_str());
  base::ScopedCFTypeRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, bundle_root_path, bundle_root->value().length(),
      true));
  base::ScopedCFTypeRef<CFBundleRef> bundle(
      CFBundleCreate(kCFAllocatorDefault, url));
  return bundle.get();
}

MockLaunchd::MockLaunchd(
    const base::FilePath& file,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::OnceClosure quit_closure,
    bool as_service)
    : file_(file),
      pipe_name_(GetServiceProcessServerName()),
      main_task_runner_(std::move(main_task_runner)),
      quit_closure_(std::move(quit_closure)),
      as_service_(as_service),
      restart_called_(false),
      remove_called_(false),
      checkin_called_(false),
      write_called_(false),
      delete_called_(false) {}

MockLaunchd::~MockLaunchd() {}

bool MockLaunchd::GetJobInfo(const std::string& label,
                             mac::services::JobInfo* info) {
  if (!as_service_) {
    std::unique_ptr<MultiProcessLock> running_lock(
        TakeNamedLock(pipe_name_, false));
    if (running_lock.get())
      return false;
  }

  info->program = file_.value();
  info->pid = base::GetCurrentProcId();
  return true;
}

bool MockLaunchd::RemoveJob(const std::string& label) {
  remove_called_ = true;
  std::move(quit_closure_).Run();
  return true;
}

bool MockLaunchd::RestartJob(Domain domain,
                             Type type,
                             CFStringRef name,
                             CFStringRef session_type) {
  restart_called_ = true;
  std::move(quit_closure_).Run();
  return true;
}

CFMutableDictionaryRef MockLaunchd::CreatePlistFromFile(Domain domain,
                                                        Type type,
                                                        CFStringRef name) {
  NSString* ns_program = base::SysUTF8ToNSString(file_.value());

  NSMutableDictionary* dict = [NSMutableDictionary dictionaryWithDictionary:@{
    @LAUNCH_JOBKEY_PROGRAM : ns_program,
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[ ns_program ],
  }];

  // Callers expect to be given a reference but dictionaryWithDictionary: is
  // autoreleased, so it's necessary to do a manual retain here.
  return base::mac::NSToCFCast([dict retain]);
}

bool MockLaunchd::WritePlistToFile(Domain domain,
                                   Type type,
                                   CFStringRef name,
                                   CFDictionaryRef dict) {
  write_called_ = true;
  return true;
}

bool MockLaunchd::DeletePlist(Domain domain, Type type, CFStringRef name) {
  delete_called_ = true;
  return true;
}

void MockLaunchd::SignalReady() {
  ASSERT_TRUE(as_service_);
  running_lock_.reset(TakeNamedLock(pipe_name_, true));
}
