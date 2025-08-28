// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ICONS_ICON_MASKER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ICONS_ICON_MASKER_H_

#include <optional>

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

using MaskedIconCallback = base::OnceCallback<void(SkBitmap)>;

// Masks the `input_bitmap` as per OS specific behavior, and
// runs the `masked_callback` in the current sequence after the masked bitmap is
// obtained. On Windows and Linux, this does not do anything, so it returns the
// input bitmap itself as a stop-gap (technically, this function shouldn't be
// called in those OSes at all). In Mac and ChromeOS, it performs masking of
// `input_bitmap` using OS specific behavior.
void MaskIconOnOs(SkBitmap input_bitmap, MaskedIconCallback masked_callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ICONS_ICON_MASKER_H_
