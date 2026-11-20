// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_TREATMENT_ARM_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_TREATMENT_ARM_H_

namespace autofill {

// The experiment arm assigned to the user for the resurrecting churned users
// experiment. This experiment arm affects the resurrection UI.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class AutofillEnableResurrectingPaymentsUsersTreatmentArm {
  // Security-focused experiment arm, resulting in a prompt that emphasizes the
  // security benefits of turning on payments autofill to the user.
  kSecurity = 0,
  // Convenience-focused experiment arm, resulting in a prompt that emphasizes
  // the convenience and speed benefits of turning on payments autofill to the
  // user.
  kConvenience = 1,
  // Message-banner experiment arm on Android, resulting in a lightweight
  // top-of-screen message banner prompting the user to turn on payments
  // autofill.
  kMessage = 2,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_TREATMENT_ARM_H_
