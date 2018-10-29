// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/test_support.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/download/quarantine/common_mac.h"
#include "url/gurl.h"

namespace download {

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& expected_source_url,
                       const GURL& referrer_url) {
  base::AssertBlockingAllowedDeprecated();

  if (!base::PathExists(file))
    return false;

  base::scoped_nsobject<NSMutableDictionary> properties;
  bool success = false;
  if (@available(macos 10.10, *))
    success = GetQuarantineProperties(file, &properties);
  else
    success = GetQuarantinePropertiesDeprecated(file, &properties);

  if (!success || !properties)
    return false;

  NSString* source_url =
      [[properties valueForKey:(NSString*)kLSQuarantineDataURLKey] description];

  if (!expected_source_url.is_valid())
    return [source_url length] > 0;

  if (![source_url isEqualToString:base::SysUTF8ToNSString(
                                       expected_source_url.spec())]) {
    return false;
  }

  return !referrer_url.is_valid() ||
         [[[properties valueForKey:(NSString*)kLSQuarantineOriginURLKey]
             description]
             isEqualToString:base::SysUTF8ToNSString(referrer_url.spec())];
}

}  // namespace download
