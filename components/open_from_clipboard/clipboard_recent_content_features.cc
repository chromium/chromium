// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_features.h"

const char kClipboardMaximumAgeParam[] = "UIClipboardMaximumAge";

// Feature used to determine the maximum age of clipboard content.
BASE_FEATURE(kClipboardMaximumAge,
             "ClipboardMaximumAge",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID)
);
