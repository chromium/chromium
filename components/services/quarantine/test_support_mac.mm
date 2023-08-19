// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/test_support.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_mac.h"
#include "url/gurl.h"

namespace quarantine {

// On macOS 12.4+ the LSQuarantineDataURL and LSQuarantineOriginURL keys are
// ignored by LaunchServices. This function will ensure the file has quarantine
// data set with kLSQuarantineAgentBundleIdentifierKey mapping to any string.
// The LSQuarantineDataURL and LSQuarantineOriginURL are treated as optional. If
// the file has them set, the function will try to match the expected values.
// Otherwise the URL matching check will be skipped.
bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& expected_source_url_unsafe,
                       const GURL& expected_referrer_url_unsafe) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(file)) {
    return false;
  }

  NSDictionary* properties = GetQuarantineProperties(file);
  if (!properties) {
    return false;
  }

  // The agent bundle id must always be set.
  NSString* bundle_id =
      [properties valueForKey:base::apple::CFToNSPtrCast(
                                  kLSQuarantineAgentBundleIdentifierKey)];
  if (!bundle_id.length) {
    return false;
  }

  // The source and referrer URLs are optional.
  GURL expected_source_url =
      SanitizeUrlForQuarantine(expected_source_url_unsafe);
  NSString* source_url = [[properties
      valueForKey:base::apple::CFToNSPtrCast(kLSQuarantineDataURLKey)]
      description];
  if (expected_source_url.is_valid() && source_url.length) {
    if (![source_url isEqualToString:base::SysUTF8ToNSString(
                                         expected_source_url.spec())]) {
      return false;
    }
  }

  GURL expected_referrer_url =
      SanitizeUrlForQuarantine(expected_referrer_url_unsafe);
  NSString* referrer_url = [[properties
      valueForKey:base::apple::CFToNSPtrCast(kLSQuarantineOriginURLKey)]
      description];
  if (expected_referrer_url.is_valid() && referrer_url.length) {
    if (![referrer_url isEqualToString:base::SysUTF8ToNSString(
                                           expected_referrer_url.spec())]) {
      return false;
    }
  }

  return true;
}

}  // namespace quarantine
