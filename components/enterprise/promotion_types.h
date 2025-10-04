// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_PROMOTION_TYPES_H_
#define COMPONENTS_ENTERPRISE_PROMOTION_TYPES_H_

namespace enterprise {

// This enum is used to store which enterprise promotion should be shown. It
// is stored as an integer in the preferences. This enum is used for
// histogram recording and should not be renumbered.
// LINT.IfChange(PromotionType)
enum class PromotionType {
  kUnspecified = 0,
  kChromeEnterpriseCore = 1,
  kChromeEnterprisePremium = 2,
  kMaxValue = kChromeEnterprisePremium,
};
// LINT.ThenChange(//components/policy/proto/device_management_backend.proto:PromotionType)

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_PROMOTION_TYPES_H_
