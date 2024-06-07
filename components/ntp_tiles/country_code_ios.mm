// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/country_code_ios.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"

std::string ntp_tiles::GetDeviceCountryCode() {
  NSString* country_code =
      [NSLocale.currentLocale objectForKey:NSLocaleCountryCode];

  return base::SysNSStringToUTF8(country_code);
}
