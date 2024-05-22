// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_

#include "components/signin/public/identity_manager/tribool.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace supervised_user {

// Returns the IsSubjectToParentalControls capability value of the primary
// account if available, and Tribool::kUnknown otherwise.
signin::Tribool IsPrimaryAccountSubjectToParentalControls(
    signin::IdentityManager* identity_manager);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_
