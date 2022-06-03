// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/field_trials_provider_helper.h"

namespace ukm {

namespace {

// UKM suffix for field trial recording.
const char kUKMFieldTrialSuffix[] = "UKM";

}  // namespace

std::unique_ptr<variations::FieldTrialsProvider>
CreateFieldTrialsProviderForUkm() {
  // TODO(crbug.com/754877): Support synthetic trials for UKM.
  return std::make_unique<variations::FieldTrialsProvider>(
      nullptr, kUKMFieldTrialSuffix);
}

}  //  namespace ukm
