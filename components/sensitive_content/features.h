// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_
#define COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace sensitive_content::features {

COMPONENT_EXPORT(SENSITIVE_CONTENT_FEATURES)
BASE_DECLARE_FEATURE(kSensitiveContent);

}  // namespace sensitive_content::features

#endif  // COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_
