// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_

#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// TODO(crbug.com/1067347): Move complete class to password_manager, once all
// references to `autofill::PasswordForm` are dropped.
using PasswordForm = autofill::PasswordForm;

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
