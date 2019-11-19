// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_
#define COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace security_state {
namespace features {

// This feature enables more aggressive warnings for nonsecure http:// pages.
// The exact warning treatment is dependent on the parameter 'treatment' which
// can have the following values:
// - 'dangerous': Treat all http:// pages as actively dangerous
// - 'warning-and-dangerous-on-form-edits': Show a Not Secure warning on all
//   http:// pages, and treat them as actively dangerous when the user edits
//   form fields
// - 'danger-warning': Show a grey triangle icon instead of the info icon on all
//   http:// pages.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kMarkHttpAsFeature;

// The parameter name which controls the warning treatment.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const char kMarkHttpAsFeatureParameterName[];

// The different parameter values, described above.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const char kMarkHttpAsParameterDangerous[];
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const char kMarkHttpAsParameterWarningAndDangerousOnFormEdits[];
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const char kMarkHttpAsParameterDangerWarning[];

// This feature enables security warning UI treatments for sites that use legacy
// TLS version (TLS 1.0 or 1.1).
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kLegacyTLSWarnings;

// This feature enables Safety Tip warnings on possibly-risky sites.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kSafetyTipUI;

}  // namespace features
}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_
