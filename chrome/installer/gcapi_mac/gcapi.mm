// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi_mac/gcapi.h"

#import <Cocoa/Cocoa.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

namespace {

// The "~~" prefixes are replaced with the home directory of the
// console owner (i.e. not the home directory of the euid).
NSString* const kChromeInstallPath = @"/Applications/Google Chrome.app";

NSString* const kBrandKey = @"KSBrandID";
NSString* const kUserBrandPath = @"~~/Library/Google/Google Chrome Brand.plist";

NSString* const kSystemKsadminPath =
    @"/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

NSString* const kUserKsadminPath =
    @"~~/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
     "Contents/MacOS/ksadmin";

NSString* const kSystemMasterPrefsPath =
    @"/Library/Google/Google Chrome Master Preferences";
NSString* const kUserMasterPrefsPath =
    @"~~/Library/Application Support/Google/Chrome/"
     "Google Chrome Master Preferences";

// Condensed from chromium's base/mac/mac_util.mm.
bool IsOSXVersionSupported() {
  // On 10.6, Gestalt() was observed to be able to spawn threads (see
  // http://crbug.com/53200). Don't call Gestalt().
  struct utsname uname_info;
  if (uname(&uname_info) != 0)
    return false;
  if (strcmp(uname_info.sysname, "Darwin") != 0)
    return false;

  char* dot = strchr(uname_info.release, '.');
  if (!dot)
    return false;

  int darwin_major_version = atoi(uname_info.release);
  if (darwin_major_version < 6)
    return false;

  // The Darwin major version is always 4 greater than the Mac OS X minor
  // version for Darwin versions beginning with 6, corresponding to Mac OS X
  // 10.2.
  int mac_os_x_minor_version = darwin_major_version - 4;

  // Chrome is known to work on 10.10 - 10.15.
  return mac_os_x_minor_version >= 10 && mac_os_x_minor_version <= 15;
}

// Returns the pid/gid of the logged-in user, even if getuid() claims that the
// current user is root.
// Returns NULL on error.
passwd* GetRealUserId() {
  CFDictionaryRef session_info_dict = CGSessionCopyCurrentDictionary();
  [NSMakeCollectable(session_info_dict) autorelease];
  if (!session_info_dict)
    return NULL;  // Possibly no screen plugged in.

  CFNumberRef ns_uid = (CFNumberRef)CFDictionaryGetValue(session_info_dict,
                                                         kCGSessionUserIDKey);
  if (CFGetTypeID(ns_uid) != CFNumberGetTypeID())
    return NULL;

  uid_t uid;
  BOOL success = CFNumberGetValue(ns_uid, kCFNumberSInt32Type, &uid);
  if (!success)
    return NULL;

  return getpwuid(uid);
}

enum TicketKind {
  kSystemTicket, kUserTicket
};

// Replaces "~~" with |home_dir|.
NSString* AdjustHomedir(NSString* s, const char* home_dir) {
  if (![s hasPrefix:@"~~"])
    return s;
  NSString* ns_home_dir = [NSString stringWithUTF8String:home_dir];
  return [ns_home_dir stringByAppendingString:[s substringFromIndex:2]];
}

// If |chrome_path| is not 0, |*chrome_path| is set to the path where chrome
// is according to keystone. It's only set if that path exists on disk.
BOOL FindChromeTicket(TicketKind kind, const passwd* user,
                      NSString** chrome_path) {
  if (chrome_path)
    *chrome_path = nil;

  // Don't use Objective-C 2 loop syntax, in case an installer runs on 10.4.
  NSMutableArray* keystone_paths =
      [NSMutableArray arrayWithObject:kSystemKsadminPath];
  if (kind == kUserTicket) {
    [keystone_paths insertObject:AdjustHomedir(kUserKsadminPath, user->pw_dir)
                        atIndex:0];
  }
  NSEnumerator* e = [keystone_paths objectEnumerator];
  id ks_path;
  while ((ks_path = [e nextObject])) {
    if (![[NSFileManager defaultManager] fileExistsAtPath:ks_path])
      continue;

    NSTask* task = nil;
    NSString* string = nil;
    bool ksadmin_ran_successfully = false;

    @try {
      task = [[NSTask alloc] init];
      [task setLaunchPath:ks_path];

      NSArray* arguments = @[
          kind == kUserTicket ? @"--user-store" : @"--system-store",
          @"--print-tickets",
          @"--productid",
          @"com.google.Chrome",
      ];
      if (geteuid() == 0 && kind == kUserTicket) {
        NSString* run_as = [NSString stringWithUTF8String:user->pw_name];
        [task setLaunchPath:@"/usr/bin/sudo"];
        arguments = [@[@"-u", run_as, ks_path]
            arrayByAddingObjectsFromArray:arguments];
      }
      [task setArguments:arguments];

      NSPipe* pipe = [NSPipe pipe];
      [task setStandardOutput:pipe];

      NSFileHandle* file = [pipe fileHandleForReading];

      [task launch];

      NSData* data = [file readDataToEndOfFile];
      [task waitUntilExit];

      ksadmin_ran_successfully = [task terminationStatus] == 0;
      string = [[[NSString alloc] initWithData:data
                                    encoding:NSUTF8StringEncoding] autorelease];
    }
    @catch (id exception) {
      // Most likely, ks_path didn't exist.
    }
    [task release];

    if (ksadmin_ran_successfully && [string length] > 0) {
      // If the user deleted chrome, it doesn't get unregistered in keystone.
      // Check if the path keystone thinks chrome is at still exists, and if not
      // treat this as "chrome isn't installed". Sniff for
      //   xc=<KSPathExistenceChecker:1234 path=/Applications/Google Chrome.app>
      // in the output. But don't mess with system tickets, since reinstalling
      // a user chrome on top of a system ticket produces a non-autoupdating
      // chrome.
      NSRange start = [string rangeOfString:@"\n\txc=<KSPathExistenceChecker:"];
      if (start.location == NSNotFound && start.length == 0)
        return YES;  // Err on the cautious side.
      string = [string substringFromIndex:start.location];

      start = [string rangeOfString:@"path="];
      if (start.location == NSNotFound && start.length == 0)
        return YES;  // Err on the cautious side.
      string = [string substringFromIndex:start.location];

      NSRange end = [string rangeOfString:@".app>\n\t"];
      if (end.location == NSNotFound && end.length == 0)
        return YES;

      string = [string substringToIndex:NSMaxRange(end) - [@">\n\t" length]];
      string = [string substringFromIndex:start.length];

      BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:string];
      if (exists && chrome_path)
        *chrome_path = string;
      // Don't allow reinstallation over a system ticket, even if chrome doesn't
      // exist on disk.
      if (kind == kSystemTicket)
        return YES;
      return exists;
    }
  }

  return NO;
}

// File permission mask for files created by gcapi.
const mode_t kUserPermissions = 0755;
const mode_t kAdminPermissions = 0775;

BOOL CreatePathToFile(NSString* path, const passwd* user) {
  path = [path stringByDeletingLastPathComponent];

  // Default owner, group, permissions:
  // * Permissions are set according to the umask of the current process. For
  //   more information, see umask.
  // * The owner ID is set to the effective user ID of the process.
  // * The group ID is set to that of the parent directory.
  // The default group ID is fine. Owner ID is fine if creating a system path,
  // but when creating a user path explicitly set the owner in case euid is 0.
  // Do set permissions explicitly; for admin paths all admins can write, for
  // user paths just the owner may.
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  if (user) {
    [attributes setObject:[NSNumber numberWithShort:kUserPermissions]
                   forKey:NSFilePosixPermissions];
    [attributes setObject:[NSNumber numberWithInt:user->pw_uid]
                   forKey:NSFileOwnerAccountID];
  } else {
    [attributes setObject:[NSNumber numberWithShort:kAdminPermissions]
                   forKey:NSFilePosixPermissions];
    [attributes setObject:@"admin" forKey:NSFileGroupOwnerAccountName];
  }

  NSFileManager* manager = [NSFileManager defaultManager];
  return [manager createDirectoryAtPath:path
            withIntermediateDirectories:YES
                             attributes:attributes
                                  error:nil];
}

// Tries to write |data| at |user_path|.
// Returns the path where it wrote, or nil on failure.
NSString* WriteUserData(NSData* data,
                        NSString* user_path,
                        const passwd* user) {
  user_path = AdjustHomedir(user_path, user->pw_dir);
  if (CreatePathToFile(user_path, user) &&
      [data writeToFile:user_path atomically:YES]) {
    chmod([user_path fileSystemRepresentation], kUserPermissions & ~0111);
    chown([user_path fileSystemRepresentation], user->pw_uid, user->pw_gid);
    return user_path;
  }
  return nil;
}

// Tries to write |data| at |system_path| or if that fails at |user_path|.
// Returns the path where it wrote, or nil on failure.
NSString* WriteData(NSData* data,
                    NSString* system_path,
                    NSString* user_path,
                    const passwd* user) {
  // Try system first.
  if (CreatePathToFile(system_path, NULL) &&
      [data writeToFile:system_path atomically:YES]) {
    chmod([system_path fileSystemRepresentation], kAdminPermissions & ~0111);
    // Make sure the file is owned by group admin.
    if (group* group = getgrnam("admin"))
      chown([system_path fileSystemRepresentation], 0, group->gr_gid);
    return system_path;
  }

  // Failed, try user.
  return WriteUserData(data, user_path, user);
}

NSString* WriteBrandCode(const char* brand_code, const passwd* user) {
  NSDictionary* brand_dict = @{
      kBrandKey: [NSString stringWithUTF8String:brand_code],
  };
  NSData* contents = [NSPropertyListSerialization
      dataFromPropertyList:brand_dict
                    format:NSPropertyListBinaryFormat_v1_0
          errorDescription:nil];

  return WriteUserData(contents, kUserBrandPath, user);
}

BOOL WriteMasterPrefs(const char* master_prefs_contents,
                      size_t master_prefs_contents_size,
                      const passwd* user) {
  NSData* contents = [NSData dataWithBytes:master_prefs_contents
                                    length:master_prefs_contents_size];
  return WriteData(
      contents, kSystemMasterPrefsPath, kUserMasterPrefsPath, user) != nil;
}

NSString* PathToFramework(NSString* app_path, NSDictionary* info_plist) {
  NSString* version = [info_plist objectForKey:@"CFBundleShortVersionString"];
  if (!version)
    return nil;
  return [NSString pathWithComponents:@[
    app_path, @"Contents", @"Frameworks", @"Google Chrome Framework.framework",
    @"Versions", version
  ]];
}

NSString* PathToInstallScript(NSString* app_path, NSDictionary* info_plist) {
  return [PathToFramework(app_path, info_plist) stringByAppendingPathComponent:
      @"Resources/install.sh"];
}

bool isbrandchar(int c) {
  // Always four upper-case alpha chars.
  return c >= 'A' && c <= 'Z';
}

}  // namespace

int GoogleChromeCompatibilityCheck(unsigned* reasons) {
  unsigned local_reasons = 0;
  @autoreleasepool {
    if (!IsOSXVersionSupported())
      local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

    NSString* path;
    if (FindChromeTicket(kSystemTicket, NULL, &path)) {
      local_reasons |= GCCC_ERROR_ALREADYPRESENT;
      if (!path)  // Ticket points to nothingness.
        local_reasons |= GCCC_ERROR_ACCESSDENIED;
    }

    passwd* user = GetRealUserId();
    if (!user)
      local_reasons |= GCCC_ERROR_ACCESSDENIED;
    else if (FindChromeTicket(kUserTicket, user, NULL))
      local_reasons |= GCCC_ERROR_ALREADYPRESENT;

    if ([[NSFileManager defaultManager] fileExistsAtPath:kChromeInstallPath])
      local_reasons |= GCCC_ERROR_ALREADYPRESENT;

    if ((local_reasons & GCCC_ERROR_ALREADYPRESENT) == 0) {
      if (![[NSFileManager defaultManager]
              isWritableFileAtPath:@"/Applications"])
      local_reasons |= GCCC_ERROR_ACCESSDENIED;
    }
  }

  if (reasons != NULL)
    *reasons = local_reasons;
  return local_reasons == 0;
}

int InstallGoogleChrome(const char* source_path,
                        const char* brand_code,
                        const char* master_prefs_contents,
                        unsigned master_prefs_contents_size) {
  if (!GoogleChromeCompatibilityCheck(NULL))
    return 0;

  @autoreleasepool {
    passwd* user = GetRealUserId();
    if (!user)
      return 0;

    NSString* app_path = [NSString stringWithUTF8String:source_path];
    NSString* info_plist_path =
        [app_path stringByAppendingPathComponent:@"Contents/Info.plist"];
    NSDictionary* info_plist =
        [NSDictionary dictionaryWithContentsOfFile:info_plist_path];

    // Use install.sh from the Chrome app bundle to copy Chrome to its
    // destination.
    NSString* install_script = PathToInstallScript(app_path, info_plist);
    if (!install_script) {
      return 0;
    }

    @try {
      NSTask* task = [[[NSTask alloc] init] autorelease];

      // install.sh tries to make the installed app admin-writable, but
      // only when it's not run as root.
      if (geteuid() == 0) {
        // Use |su $(whoami)| instead of sudo -u. If the current user is in more
        // than 16 groups, |sudo -u $(whoami)| will drop all but the first 16
        // groups, which can lead to problems (e.g. if "admin" is one of the
        // dropped groups).
        // Since geteuid() is 0, su won't prompt for a password.
        NSString* run_as = [NSString stringWithUTF8String:user->pw_name];
        [task setLaunchPath:@"/usr/bin/su"];

        NSString* single_quote_escape = @"'\"'\"'";
        NSString* install_script_quoted = [install_script
            stringByReplacingOccurrencesOfString:@"'"
                                      withString:single_quote_escape];
        NSString* app_path_quoted =
            [app_path stringByReplacingOccurrencesOfString:@"'"
                                                withString:single_quote_escape];
        NSString* install_path_quoted = [kChromeInstallPath
            stringByReplacingOccurrencesOfString:@"'"
                                      withString:single_quote_escape];

        NSString* install_script_execution =
            [NSString stringWithFormat:@"exec '%@' '%@' '%@'",
                                       install_script_quoted,
                                       app_path_quoted,
                                       install_path_quoted];
        [task setArguments:
            @[run_as, @"-c", install_script_execution]];
      } else {
        [task setLaunchPath:install_script];
        [task setArguments:@[app_path, kChromeInstallPath]];
      }

      [task launch];
      [task waitUntilExit];
      if ([task terminationStatus] != 0) {
        return 0;
      }
    }
    @catch (id exception) {
      return 0;
    }

    // Set brand code. If Chrome's Info.plist contains a brand code, use that.
    NSString* info_plist_brand = [info_plist objectForKey:kBrandKey];
    if (info_plist_brand &&
        [info_plist_brand respondsToSelector:@selector(UTF8String)])
      brand_code = [info_plist_brand UTF8String];

    BOOL valid_brand_code = brand_code && strlen(brand_code) == 4 &&
        isbrandchar(brand_code[0]) && isbrandchar(brand_code[1]) &&
        isbrandchar(brand_code[2]) && isbrandchar(brand_code[3]);

    NSString* brand_path = nil;
    if (valid_brand_code)
      brand_path = WriteBrandCode(brand_code, user);

    // Write master prefs.
    if (master_prefs_contents)
      WriteMasterPrefs(master_prefs_contents, master_prefs_contents_size, user);

    // TODO Set default browser if requested.
  }
  return 1;
}

int LaunchGoogleChrome() {
  @autoreleasepool {
    passwd* user = GetRealUserId();
    if (!user)
      return 0;

    NSString* app_path;

    NSString* path;
    if (FindChromeTicket(kUserTicket, user, &path) && path)
      app_path = path;
    else if (FindChromeTicket(kSystemTicket, NULL, &path) && path)
      app_path = path;
    else
      app_path = kChromeInstallPath;

    // NSWorkspace launches processes as the current console owner,
    // even when running with euid of 0.
    return [[NSWorkspace sharedWorkspace] launchApplication:app_path];
  }
}
