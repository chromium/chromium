// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/features.h"

namespace sensitive_content::features {

// When enabled, the content view will be redacted if sensitive fields (such as
// passwords or credit cards) are present on the page. The feature works only on
// Android API level >= 35.
// TODO(crbug.com/343119998): Remove once launched.
BASE_FEATURE(kSensitiveContent,
             "SensitiveContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sensitive_content::features
