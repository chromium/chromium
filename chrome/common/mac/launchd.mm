// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/launchd.h"

#import <Foundation/Foundation.h>
#include <launch.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/mac/service_management.h"

namespace {

NSString* SanitizeShellArgument(NSString* arg) {
  if (!arg) {
    return nil;
  }
  NSString *sanitize = [arg stringByReplacingOccurrencesOfString:@"'"
                                                      withString:@"'\''"];
  return [NSString stringWithFormat:@"'%@'", sanitize];
}

NSURL* GetPlistURL(Launchd::Domain domain,
                   Launchd::Type type,
                   CFStringRef name) {
  NSArray* library_paths =
      NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, domain, YES);
  DCHECK_EQ([library_paths count], 1U);
  NSString* library_path = [library_paths objectAtIndex:0];

  NSString *launch_dir_name = (type == Launchd::Daemon) ? @"LaunchDaemons"
                                                        : @"LaunchAgents";
  NSString* launch_dir =
      [library_path stringByAppendingPathComponent:launch_dir_name];

  NSError* err;
  if (![[NSFileManager defaultManager] createDirectoryAtPath:launch_dir
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:&err]) {
    DLOG(ERROR) << "GetPlistURL " << base::mac::NSToCFCast(err);
    return nil;
  }

  NSString* plist_file_path =
      [launch_dir stringByAppendingPathComponent:base::mac::CFToNSCast(name)];
  plist_file_path = [plist_file_path stringByAppendingPathExtension:@"plist"];
  return [NSURL fileURLWithPath:plist_file_path isDirectory:NO];
}

}  // namespace

static_assert(static_cast<int>(Launchd::User) ==
              static_cast<int>(NSUserDomainMask),
              "NSUserDomainMask value changed");
static_assert(static_cast<int>(Launchd::Local) ==
              static_cast<int>(NSLocalDomainMask),
              "NSLocalDomainMask value changed");
static_assert(static_cast<int>(Launchd::Network) ==
              static_cast<int>(NSNetworkDomainMask),
              "NSNetworkDomainMask value changed");
static_assert(static_cast<int>(Launchd::System) ==
              static_cast<int>(NSSystemDomainMask),
              "NSSystemDomainMask value changed");

Launchd* Launchd::g_instance_ = NULL;

Launchd* Launchd::GetInstance() {
  if (!g_instance_) {
    g_instance_ = base::Singleton<Launchd>::get();
  }
  return g_instance_;
}

void Launchd::SetInstance(Launchd* instance) {
  if (instance) {
    CHECK(!g_instance_);
  }
  g_instance_ = instance;
}

Launchd::~Launchd() { }

bool Launchd::GetJobInfo(const std::string& label,
                         mac::services::JobInfo* info) {
  return mac::services::GetJobInfo(label, info);
}

bool Launchd::RemoveJob(const std::string& label) {
  return mac::services::RemoveJob(label);
}

bool Launchd::RestartJob(Domain domain,
                         Type type,
                         CFStringRef name,
                         CFStringRef cf_session_type) {
  @autoreleasepool {
    NSURL* url = GetPlistURL(domain, type, name);
    NSString* ns_path = [url path];
    ns_path = SanitizeShellArgument(ns_path);
    const char* file_path = [ns_path fileSystemRepresentation];

    NSString* ns_session_type =
        SanitizeShellArgument(base::mac::CFToNSCast(cf_session_type));
    if (!file_path || !ns_session_type) {
      return false;
    }

    std::vector<std::string> argv;
    argv.push_back("/bin/bash");
    argv.push_back("--noprofile");
    argv.push_back("-c");
    std::string command =
        base::StringPrintf("/bin/launchctl unload -S %s %s;"
                           "/bin/launchctl load -S %s %s;",
                           [ns_session_type UTF8String], file_path,
                           [ns_session_type UTF8String], file_path);
    argv.push_back(command);

    base::LaunchOptions options;
    options.new_process_group = true;
    return base::LaunchProcess(argv, options).IsValid();
  }
}

CFMutableDictionaryRef Launchd::CreatePlistFromFile(Domain domain,
                                                    Type type,
                                                    CFStringRef name) {
  @autoreleasepool {
    NSURL* ns_url = GetPlistURL(domain, type, name);
    NSMutableDictionary* plist =
        [[NSMutableDictionary alloc] initWithContentsOfURL:ns_url];
    return base::mac::NSToCFCast(plist);
  }
}

bool Launchd::WritePlistToFile(Domain domain,
                               Type type,
                               CFStringRef name,
                               CFDictionaryRef dict) {
  @autoreleasepool {
    NSURL* ns_url = GetPlistURL(domain, type, name);
    return [base::mac::CFToNSCast(dict) writeToURL:ns_url atomically:YES];
  }
}

bool Launchd::DeletePlist(Domain domain, Type type, CFStringRef name) {
  @autoreleasepool {
    NSURL* ns_url = GetPlistURL(domain, type, name);
    NSError* err = nil;
    if (![[NSFileManager defaultManager] removeItemAtPath:[ns_url path]
                                                    error:&err]) {
      if ([err code] != NSFileNoSuchFileError) {
        DLOG(ERROR) << "DeletePlist: " << base::mac::NSToCFCast(err);
      }
      return false;
    }
    return true;
  }
}
