// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_features.h"

const char kClipboardMaximumAgeParam[] = "UIClipboardMaximumAge";

// Feature used to determine the maximum age of clipboard content.
const base::Feature kClipboardMaximumAge{"ClipboardMaximumAge",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
