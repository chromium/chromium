// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FORWARD_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FORWARD_H_

// While `password_manager::PasswordForm` is just an alias to
// `autofill::PasswordForm` it is rather awkward to construct a forward
// declaration for it, since a naive solution such as
// namespace password_manager { struct PasswordForm; } would break the build.
// This file aims to make this easier, and classes merely interested in a
// forward declaration can simply #include this header.
//
// TODO(crbug.com/1067347): Remove this file once password_manager::PasswordForm
// is a real class, and a regular forward declaration does work.
namespace autofill {
struct PasswordForm;
}

namespace password_manager {
using PasswordForm = autofill::PasswordForm;
}

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FORWARD_H_
