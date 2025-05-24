// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_

#include "base/observer_list_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {
class PasswordManagerDriver;
struct PasswordForm;
class PasswordFormManager;

// Allows observing events inside PasswordFormManager. Declared here to allow
// adding observers to all form mangers in the cache.
class PasswordFormManagerObserver {
 public:
  virtual ~PasswordFormManagerObserver() {}

  // Invoked whenever `form_manager` parses a form.
  virtual void OnPasswordFormParsed(PasswordFormManager* form_manager) = 0;
};

// Contains information about password forms detected on a web page.
class PasswordFormCache {
 public:
  PasswordFormCache();
  virtual ~PasswordFormCache();
  PasswordFormCache(const PasswordFormCache&) = delete;
  PasswordFormCache& operator=(const PasswordFormCache&) = delete;

  // If present, returns a `PasswordForm` for the given `driver` and `form_id`,
  // and `nullptr` otherwise.
  virtual const PasswordForm* GetPasswordForm(
      PasswordManagerDriver* driver,
      autofill::FormRendererId form_id) const = 0;
  // If present, returns a `PasswordForm` for the given `driver` and `field_id`,
  // and `nullptr` otherwise.
  virtual const PasswordForm* GetPasswordForm(
      PasswordManagerDriver* driver,
      autofill::FieldRendererId field_id) const = 0;

  // Allows adding an observer for all newly added password form managers.
  virtual void SetObserver(
      base::WeakPtr<PasswordFormManagerObserver> observer) {}

  // Removes observer from all current form managers and prevents attaching
  // observer to newly added.
  virtual void ResetObserver() {}
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_
