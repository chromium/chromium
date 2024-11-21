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

// Contains information about password forms detected on a web page.
class PasswordFormCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked whenever `form_manager` is added to the cache.
    virtual void OnFormManagerAdded(PasswordFormManager* form_manager) {}
  };

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

  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_H_
