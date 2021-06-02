// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_

namespace accuracy_tips {

// Represents the different results of the accuracy check.
enum class AccuracyTipStatus {
  // No accuracy information for the site.
  kNone = 0,
  // Site classified as being misleading.
  kMisinformation = 1,
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_STATUS_H_