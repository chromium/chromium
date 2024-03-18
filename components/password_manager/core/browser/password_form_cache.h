// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_

#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {
class PasswordManagerDriver;

// Contains information about password forms detected on a web page.
class PasswordFormCache {
 public:
  PasswordFormCache();
  virtual ~PasswordFormCache();
  PasswordFormCache(const PasswordFormCache&) = delete;
  PasswordFormCache& operator=(const PasswordFormCache&) = delete;

  // Checks if this cache contains a password form identified by the `form_id`.
  virtual bool HasPasswordForm(PasswordManagerDriver* driver,
                               autofill::FormRendererId form_id) const = 0;
  // Checks if this cache contains a password form with a field identified by
  // the `field_id`.
  virtual bool HasPasswordForm(PasswordManagerDriver* driver,
                               autofill::FieldRendererId field_id) const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_
