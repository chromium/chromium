// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash::assistant::features {

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
BASE_DECLARE_FEATURE(kEnableNewEntryPoint);

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsNewEntryPointEnabled();

}  // namespace ash::assistant::features

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
