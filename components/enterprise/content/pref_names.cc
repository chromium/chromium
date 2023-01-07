// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/content/pref_names.h"

namespace enterprise {
namespace content {

// Dictionary policy describing a set of URL patterns where copying to the
// clipboard is not allowed.
const char kCopyPreventionSettings[] = "policy.copy_prevention_settings";
const char kCopyPreventionSettingsEnableFieldName[] = "enable";
const char kCopyPreventionSettingsDisableFieldName[] = "disable";
const char kCopyPreventionSettingsMinDataSizeFieldName[] = "minimum_data_size";

}  // namespace content
}  // namespace enterprise