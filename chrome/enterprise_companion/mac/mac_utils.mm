// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/mac/mac_utils.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/types.h>

#include <optional>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"

namespace enterprise_companion {

std::optional<uid_t> GuessLoggedInUser() {
  base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      nullptr, CFSTR(PRODUCT_FULLNAME_STRING), nullptr, nullptr));
  if (!store) {
    LOG(ERROR) << "SCDynamicStoreCreate failed";
    return std::nullopt;
  }

  base::apple::ScopedCFTypeRef<CFPropertyListRef> plist(
      SCDynamicStoreCopyValue(store.get(), CFSTR("State:/Users/ConsoleUser")));
  if (!plist) {
    LOG(ERROR) << "SCDynamicStoreCopyValue failed";
    return std::nullopt;
  }

  NSDictionary* plist_dict = base::apple::CFToNSPtrCast(
      base::apple::CFCast<CFDictionaryRef>(plist.get()));
  if (!plist_dict) {
    LOG(ERROR) << "plist not a dictionary.";
    return std::nullopt;
  }

  NSArray<NSDictionary*>* session_info_array =
      base::apple::ObjCCast<NSArray>(plist_dict[@"SessionInfo"]);
  if (!session_info_array) {
    LOG(ERROR) << "SessionInfo not NSArray";
    return std::nullopt;
  }

  for (NSDictionary* session_dict in session_info_array) {
    NSNumber* is_console_session = base::apple::ObjCCast<NSNumber>(
        session_dict[base::apple::CFToNSPtrCast(kCGSessionOnConsoleKey)]);
    if (!is_console_session) {
      LOG(ERROR) << "kCGSSessionOnConsoleKey not NSNumber";
      continue;
    }

    if (is_console_session.boolValue) {
      NSNumber* uid = base::apple::ObjCCast<NSNumber>(
          session_dict[base::apple::CFToNSPtrCast(kCGSessionUserIDKey)]);
      if (!uid) {
        LOG(ERROR) << "kCGSSessionUserIDKey not NSNumber";
        continue;
      }
      return uid.unsignedIntValue;
    }
  }

  return std::nullopt;
}

}  // namespace enterprise_companion
