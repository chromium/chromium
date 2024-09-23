// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {
class PasswordManagerDriver;

// Implementation of the `PasswordFormCache` that stores parsed password forms
// in the form of a vector of `PasswordFormManager` instances.
class PasswordFormCacheImpl : public PasswordFormCache {
 public:
  PasswordFormCacheImpl();
  ~PasswordFormCacheImpl() override;

  PasswordFormManager* GetMatchedManager(
      PasswordManagerDriver* driver,
      autofill::FormRendererId form_id) const;
  PasswordFormManager* GetMatchedManager(
      PasswordManagerDriver* driver,
      autofill::FieldRendererId field_id) const;

  void AddFormManager(std::unique_ptr<PasswordFormManager> manager);
  void ResetSubmittedManager();
  PasswordFormManager* GetSubmittedManager() const;
  std::unique_ptr<PasswordFormManager> MoveOwnedSubmittedManager();
  void Clear();
  bool IsEmpty() const;

  base::span<const std::unique_ptr<PasswordFormManager>> GetFormManagers()
      const;

 private:
  // PasswordFormCache:
  const PasswordForm* GetPasswordForm(
      PasswordManagerDriver* driver,
      autofill::FormRendererId form_id) const override;
  const PasswordForm* GetPasswordForm(
      PasswordManagerDriver* driver,
      autofill::FieldRendererId field_id) const override;

  // TODO(b/330313855): Check if `unique_ptr` can be removed here.
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_CACHE_IMPL_H_
