// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_CALLBACK_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_CALLBACK_H_

#include <vector>

#include "base/callback.h"

namespace favicon_base {

struct FaviconRawBitmapResult;
struct FaviconImageResult;
struct LargeIconResult;
struct LargeIconImageResult;
enum class GoogleFaviconServerRequestStatus;

// Callback for functions that can be used to return a |gfx::Image| and the
// |GURL| it is loaded from. They are returned as a |FaviconImageResult| object.
typedef base::OnceCallback<void(const FaviconImageResult&)>
    FaviconImageCallback;

// Callback for functions returning raw data for a favicon. In
// |FaviconRawBitmapResult|, the data is not yet converted as a |gfx::Image|.
typedef base::OnceCallback<void(const FaviconRawBitmapResult&)>
    FaviconRawBitmapCallback;

// Callback for functions returning raw data for a favicon in multiple
// resolution. In |FaviconRawBitmapResult|, the data is not yet converted as a
// |gfx::Image|.
typedef base::OnceCallback<void(const std::vector<FaviconRawBitmapResult>&)>
    FaviconResultsCallback;

// Callback for functions returning data for a large icon. |LargeIconResult|
// will contain either the raw bitmap for a large icon or the style of the
// fallback to use if a sufficiently large icon could not be found.
// TODO(jkrcal): Rename LargeIcon* to LargeIconRawBitmap*.
typedef base::OnceCallback<void(const LargeIconResult&)> LargeIconCallback;

// Callback for functions returning decoded data for a large icon.
// |LargeIconImageResult| will contain either the decoded image of a large
// icon or the style of the fallback to use if a sufficiently large icon could
// not be found.
typedef base::OnceCallback<void(const LargeIconImageResult&)>
    LargeIconImageCallback;

typedef base::OnceCallback<void(GoogleFaviconServerRequestStatus)>
    GoogleFaviconServerCallback;

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_CALLBACK_H_
