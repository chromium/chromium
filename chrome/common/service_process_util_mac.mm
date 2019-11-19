// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <launch.h>
#include <sys/un.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/service_process_util_posix.h"
#include "components/version_info/version_info.h"

using ::base::FilePathWatcher;

namespace {

#define kServiceProcessSessionType "Aqua"

CFStringRef CopyServiceProcessLaunchDName() {
  @autoreleasepool {
    NSBundle* bundle = base::mac::FrameworkBundle();
    return CFStringCreateCopy(kCFAllocatorDefault,
                              base::mac::NSToCFCast([bundle bundleIdentifier]));
  }
}

NSString* GetServiceProcessLaunchDLabel() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyServiceProcessLaunchDName()));
  NSString* label = [name stringByAppendingString:@".service_process"];
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  std::string user_data_dir_path = user_data_dir.value();
  NSString* ns_path = base::SysUTF8ToNSString(user_data_dir_path);
  ns_path = [ns_path stringByReplacingOccurrencesOfString:@" "
                                               withString:@"_"];
  label = [label stringByAppendingString:ns_path];
  return label;
}

bool RemoveFromLaunchd() {
  // We're killing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
  return Launchd::GetInstance()->DeletePlist(Launchd::User,
                                             Launchd::Agent,
                                             name);
}

class ExecFilePathWatcherCallback {
 public:
  ExecFilePathWatcherCallback() {}
  ~ExecFilePathWatcherCallback() {}

  bool Init(const base::FilePath& path);
  void NotifyPathChanged(const base::FilePath& path, bool error);

 private:
  base::scoped_nsobject<NSURL> executable_fsref_;
};

NSString* GetServiceProcessMachName() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyServiceProcessLaunchDName()));
  return [name stringByAppendingFormat:@".service_process.%lu",
                                       [GetServiceProcessLaunchDLabel() hash]];
}

}  // namespace

mojo::NamedPlatformChannel::ServerName GetServiceProcessServerName() {
  return base::SysNSStringToUTF8(GetServiceProcessMachName());
}

bool ForceServiceProcessShutdown(const std::string& /* version */,
                                 base::ProcessId /* process_id */) {
  const std::string& label =
      base::SysNSStringToUTF8(GetServiceProcessLaunchDLabel());
  bool ret = Launchd::GetInstance()->RemoveJob(label);
  if (!ret) {
    DLOG(ERROR) << "ForceServiceProcessShutdown: " << label;
  }
  return ret;
}

bool ServiceProcessState::GetServiceProcessData(std::string* version,
                                                base::ProcessId* pid) {
  @autoreleasepool {
    std::string label =
        base::SysNSStringToUTF8(GetServiceProcessLaunchDLabel());
    mac::services::JobInfo info;
    if (!Launchd::GetInstance()->GetJobInfo(label, &info))
      return false;
    // Anything past here will return true in that there does appear
    // to be a service process of some sort registered with launchd.
    if (version) {
      *version = "0";
      NSString* exe_path = base::SysUTF8ToNSString(info.program);
      if (exe_path) {
        NSString* bundle_path = [[[exe_path stringByDeletingLastPathComponent]
            stringByDeletingLastPathComponent]
            stringByDeletingLastPathComponent];
        NSBundle* bundle = [NSBundle bundleWithPath:bundle_path];
        if (bundle) {
          NSString* ns_version =
              [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
          if (ns_version) {
            *version = base::SysNSStringToUTF8(ns_version);
          } else {
            DLOG(ERROR) << "Unable to get version at: "
                        << reinterpret_cast<CFStringRef>(bundle_path);
          }
        } else {
          // The bundle has been deleted out from underneath the registered
          // job.
          DLOG(ERROR) << "Unable to get bundle at: "
                      << reinterpret_cast<CFStringRef>(bundle_path);
        }
      } else {
        DLOG(ERROR) << "Unable to get executable path for service process";
      }
    }
    if (pid) {
      *pid = info.pid ? *info.pid : -1;
    }
    return true;
  }
}

bool ServiceProcessState::Initialize() {
  mac::services::JobInfo info;
  // The Mach service will be checked when GetServiceProcessServerEndpoint()
  // is called.
  bool ok = Launchd::GetInstance()->GetJobInfo(
      base::SysNSStringToUTF8(GetServiceProcessLaunchDLabel()), &info);
  if (!ok) {
    DLOG(ERROR) << "Failed to look up job info.";
    return false;
  }
  state_->job_info.program = info.program;
  return true;
}

mojo::PlatformChannelServerEndpoint
ServiceProcessState::GetServiceProcessServerEndpoint() {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = base::SysNSStringToUTF8(GetServiceProcessMachName());
  return mojo::NamedPlatformChannel(options).TakeServerEndpoint();
}

bool CheckServiceProcessReady() {
  std::string version;
  pid_t pid;
  if (!ServiceProcessState::GetServiceProcessData(&version, &pid)) {
    return false;
  }
  base::Version service_version(version);
  bool ready = true;
  if (!service_version.IsValid()) {
    ready = false;
  } else {
    const base::Version& running_version = version_info::GetVersion();
    if (!running_version.IsValid()) {
      // Our own version is invalid. This is an error case. Pretend that we
      // are out of date.
      NOTREACHED();
      ready = true;
    } else {
      ready = running_version.CompareTo(service_version) <= 0;
    }
  }
  if (!ready) {
    ForceServiceProcessShutdown(version, pid);
  }
  return ready;
}

mac::services::JobOptions GetServiceProcessJobOptions(
    base::CommandLine* cmd_line,
    bool for_auto_launch) {
  mac::services::JobOptions options;

  options.label = base::SysNSStringToUTF8(GetServiceProcessLaunchDLabel());
  options.executable_path = cmd_line->GetProgram().value();
  options.arguments = cmd_line->argv();
  options.mach_service_name =
      base::SysNSStringToUTF8(GetServiceProcessMachName());
  options.run_at_load = for_auto_launch;
  options.auto_launch = for_auto_launch;

  return options;
}

CFDictionaryRef CreateServiceProcessLaunchdPlist(base::CommandLine* cmd_line,
                                                 bool for_auto_launch) {
  @autoreleasepool {
    NSString* program = base::SysUTF8ToNSString(cmd_line->GetProgram().value());

    std::vector<std::string> args = cmd_line->argv();
    NSMutableArray* ns_args = [NSMutableArray arrayWithCapacity:args.size()];

    for (std::vector<std::string>::iterator iter = args.begin();
         iter < args.end(); ++iter) {
      [ns_args addObject:base::SysUTF8ToNSString(*iter)];
    }

    // See the man page for launchd.plist.
    NSMutableDictionary* launchd_plist = [@{
      @LAUNCH_JOBKEY_LABEL : GetServiceProcessLaunchDLabel(),
      @LAUNCH_JOBKEY_PROGRAM : program,
      @LAUNCH_JOBKEY_PROGRAMARGUMENTS : ns_args,
      @LAUNCH_JOBKEY_MACHSERVICES : GetServiceProcessMachName(),
    } mutableCopy];

    if (for_auto_launch) {
      // We want the service process to be able to exit if there are no services
      // enabled. With a value of NO in the SuccessfulExit key, launchd will
      // relaunch the service automatically in any other case than exiting
      // cleanly with a 0 return code.
      NSDictionary* keep_alive =
          @{@LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT : @NO};
      NSDictionary* auto_launchd_plist = @{
        @LAUNCH_JOBKEY_RUNATLOAD : @YES,
        @LAUNCH_JOBKEY_KEEPALIVE : keep_alive,
        @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @kServiceProcessSessionType
      };
      [launchd_plist addEntriesFromDictionary:auto_launchd_plist];
    }
    return reinterpret_cast<CFDictionaryRef>(launchd_plist);
  }
}

// Writes the launchd property list into the user's LaunchAgents directory,
// creating that directory if needed. This will cause the service process to be
// auto launched on the next user login.
bool ServiceProcessState::AddToAutoRun() {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(autorun_command_line_.get());
  base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateServiceProcessLaunchdPlist(autorun_command_line_.get(), true));
  return Launchd::GetInstance()->WritePlistToFile(Launchd::User,
                                                  Launchd::Agent,
                                                  name,
                                                  plist);
}

bool ServiceProcessState::RemoveFromAutoRun() {
  return RemoveFromLaunchd();
}

bool ServiceProcessState::StateData::WatchExecutable() {
  @autoreleasepool {
    base::FilePath executable_path = base::FilePath(job_info.program);
    std::unique_ptr<ExecFilePathWatcherCallback> callback(
        new ExecFilePathWatcherCallback);
    if (!callback->Init(executable_path)) {
      DLOG(ERROR) << "executable_watcher.Init " << executable_path.value();
      return false;
    }
    if (!executable_watcher.Watch(
            executable_path, false,
            base::Bind(&ExecFilePathWatcherCallback::NotifyPathChanged,
                       base::Owned(callback.release())))) {
      DLOG(ERROR) << "executable_watcher.watch " << executable_path.value();
      return false;
    }
    return true;
  }
}

bool ExecFilePathWatcherCallback::Init(const base::FilePath& path) {
  NSString* path_string = base::mac::FilePathToNSString(path);
  NSURL* path_url = [NSURL fileURLWithPath:path_string isDirectory:NO];
  executable_fsref_.reset([[path_url fileReferenceURL] retain]);
  return executable_fsref_.get() != nil;
}

void ExecFilePathWatcherCallback::NotifyPathChanged(const base::FilePath& path,
                                                    bool error) {
  if (error) {
    NOTREACHED();  // TODO(darin): Do something smarter?
    return;
  }

  @autoreleasepool {
    bool needs_shutdown = false;
    bool needs_restart = false;
    bool good_bundle = false;

    // Go from bundle/Contents/MacOS/executable to bundle.
    NSURL* bundle_url = [[[executable_fsref_ URLByDeletingLastPathComponent]
        URLByDeletingLastPathComponent] URLByDeletingLastPathComponent];
    if (bundle_url) {
      base::ScopedCFTypeRef<CFBundleRef> bundle(CFBundleCreate(
          kCFAllocatorDefault, base::mac::NSToCFCast(bundle_url)));
      good_bundle = CFBundleGetIdentifier(bundle) != NULL;
    }

    if (!good_bundle) {
      needs_shutdown = true;
    } else {
      bool in_trash = false;
      NSFileManager* file_manager = [NSFileManager defaultManager];
      NSURLRelationship relationship;
      if ([file_manager getRelationship:&relationship
                            ofDirectory:NSTrashDirectory
                               inDomain:0
                            toItemAtURL:executable_fsref_
                                  error:nil]) {
        in_trash = relationship == NSURLRelationshipContains;
      }
      if (in_trash) {
        needs_shutdown = true;
      } else {
        bool was_moved = true;
        NSString* path_string = base::mac::FilePathToNSString(path);
        NSURL* path_url = [NSURL fileURLWithPath:path_string isDirectory:NO];
        NSURL* path_ref = [path_url fileReferenceURL];
        if (path_ref != nil) {
          if ([path_ref isEqual:executable_fsref_]) {
            was_moved = false;
          }
        }
        if (was_moved) {
          needs_restart = true;
        }
      }
    }
    if (needs_shutdown || needs_restart) {
      // First deal with the plist.
      base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
      if (needs_restart) {
        base::ScopedCFTypeRef<CFMutableDictionaryRef> plist(
            Launchd::GetInstance()->CreatePlistFromFile(Launchd::User,
                                                        Launchd::Agent, name));
        if (plist.get()) {
          NSMutableDictionary* ns_plist = base::mac::CFToNSCast(plist);
          NSURL* new_path = [executable_fsref_ filePathURL];
          DCHECK([new_path isFileURL]);
          NSString* ns_new_path = [new_path path];
          ns_plist[@LAUNCH_JOBKEY_PROGRAM] = ns_new_path;
          base::scoped_nsobject<NSMutableArray> args(
              [ns_plist[@LAUNCH_JOBKEY_PROGRAMARGUMENTS] mutableCopy]);
          args[0] = ns_new_path;
          ns_plist[@LAUNCH_JOBKEY_PROGRAMARGUMENTS] = args;
          if (!Launchd::GetInstance()->WritePlistToFile(
                  Launchd::User, Launchd::Agent, name, plist)) {
            DLOG(ERROR) << "Unable to rewrite plist.";
            needs_shutdown = true;
          }
        } else {
          DLOG(ERROR) << "Unable to read plist.";
          needs_shutdown = true;
        }
      }
      if (needs_shutdown) {
        if (!RemoveFromLaunchd()) {
          DLOG(ERROR) << "Unable to RemoveFromLaunchd.";
        }
      }

      // Then deal with the process.
      CFStringRef session_type = CFSTR(kServiceProcessSessionType);
      if (needs_restart) {
        if (!Launchd::GetInstance()->RestartJob(Launchd::User, Launchd::Agent,
                                                name, session_type)) {
          DLOG(ERROR) << "RestartLaunchdJob";
          needs_shutdown = true;
        }
      }
      if (needs_shutdown) {
        const std::string& label =
            base::SysNSStringToUTF8(GetServiceProcessLaunchDLabel());
        if (!Launchd::GetInstance()->RemoveJob(label)) {
          DLOG(ERROR) << "RemoveJob " << label;
          // Exiting with zero, so launchd doesn't restart the process.
          exit(0);
        }
      }
    }
  }
}
