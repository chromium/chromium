// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEATURES_H_
#define COMPONENTS_FEEDBACK_FEATURES_H_

#include "base/feature_list.h"

namespace feedback::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// Alphabetical:
extern const base::Feature kOsFeedbackSaveReportToLocalForE2ETesting;

extern bool IsOsFeedbackSaveReportToLocalForE2ETestingEnabled();

}  // namespace feedback::features

#endif  // COMPONENTS_FEEDBACK_FEATURES_H_
