// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_SODA_FEATURES_H_
#define COMPONENTS_SODA_SODA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace speech {
#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(SODA_INSTALLER)
BASE_DECLARE_FEATURE(kCrosExpandSodaLanguages);
COMPONENT_EXPORT(SODA_INSTALLER)
BASE_DECLARE_FEATURE(kFeatureManagementCrosSodaConchLanguages);
COMPONENT_EXPORT(SODA_INSTALLER)
BASE_DECLARE_FEATURE(kCrosSodaConchLanguages);
#endif
}  // namespace speech

#endif  // COMPONENTS_SODA_SODA_FEATURES_H_
