// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace payments::facilitated {

BASE_DECLARE_FEATURE(kEnablePixDetection);
BASE_DECLARE_FEATURE(kEnablePixDetectionOnDomContentLoaded);
BASE_DECLARE_FEATURE(kEnablePixPayments);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_
