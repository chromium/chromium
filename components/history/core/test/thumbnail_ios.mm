// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/thumbnail.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/history/core/test/thumbnail-inl.h"
#include "ui/gfx/image/image.h"

namespace history {

gfx::Image CreateGoogleThumbnailForTest() {
  @autoreleasepool {
    // -[NSData dataWithBytesNoCopy:length:freeWhenDone:] takes its first
    // parameter as a void* but does not modify it (API is not const clean) so
    // we need to use const_cast<> here.
    NSData* data = [NSData
        dataWithBytesNoCopy:const_cast<void*>(
                                static_cast<const void*>(kGoogleThumbnail))
                     length:sizeof(kGoogleThumbnail)
               freeWhenDone:NO];
    return gfx::Image([UIImage imageWithData:data scale:1]);
  }
}

}  // namespace
