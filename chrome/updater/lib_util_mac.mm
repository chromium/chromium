// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lib_util.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

namespace updater {

std::string UnescapeURLComponent(base::StringPiece escaped_text_piece) {
  @autoreleasepool {
    NSString* escaped_text = base::SysUTF8ToNSString(escaped_text_piece);

    // Escape stray percent signs not followed by a hex byte to match the //net
    // and Windows' ::UnescapeUrlA behavior of ignoring invalid percent codes.
    NSRegularExpression* regex = [NSRegularExpression
        regularExpressionWithPattern:@"%(?![a-f0-9]{2})"
                             options:NSRegularExpressionCaseInsensitive
                               error:nil];
    escaped_text = [regex
        stringByReplacingMatchesInString:escaped_text
                                 options:0
                                   range:NSMakeRange(0, [escaped_text length])
                            withTemplate:@"%25"];

    NSString* unescaped_text = [escaped_text stringByRemovingPercentEncoding];
    return base::SysNSStringToUTF8(unescaped_text);
  }
}

}  // namespace updater
