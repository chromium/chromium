// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/test_support.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_mac.h"
#include "url/gurl.h"

namespace quarantine {

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& expected_source_url_unsafe,
                       const GURL& expected_referrer_url_unsafe) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(file))
    return false;

  base::scoped_nsobject<NSMutableDictionary> properties;
  bool success = GetQuarantineProperties(file, &properties);

  if (!success || !properties)
    return false;

  GURL expected_source_url =
      SanitizeUrlForQuarantine(expected_source_url_unsafe);
  GURL expected_referrer_url =
      SanitizeUrlForQuarantine(expected_referrer_url_unsafe);

  NSString* source_url =
      [[properties valueForKey:(NSString*)kLSQuarantineDataURLKey] description];

  if (!expected_source_url.is_valid())
    return [source_url length] > 0;

  if (![source_url isEqualToString:base::SysUTF8ToNSString(
                                       expected_source_url.spec())]) {
    return false;
  }

  return !expected_referrer_url.is_valid() ||
         [[[properties valueForKey:(NSString*)kLSQuarantineOriginURLKey]
             description]
             isEqualToString:base::SysUTF8ToNSString(
                                 expected_referrer_url.spec())];
}

}  // namespace quarantine
