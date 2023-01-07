// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "components/ntp_tiles/country_code_ios.h"

std::string ntp_tiles::GetDeviceCountryCode() {
  NSLocale *current_locale = [NSLocale currentLocale];
  NSString *country_code = [current_locale objectForKey:NSLocaleCountryCode];

  return base::SysNSStringToUTF8(country_code);
}
