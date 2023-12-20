// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/form_saver.h"

namespace password_manager {

class PasswordStoreInterface;

// The production code implementation of FormSaver.
class FormSaverImpl : public FormSaver {
 public:
  // |store| needs to outlive |this| and will be used for all
  // PasswordStoreInterface operations.
  explicit FormSaverImpl(PasswordStoreInterface* store);

  FormSaverImpl(const FormSaverImpl&) = delete;
  FormSaverImpl& operator=(const FormSaverImpl&) = delete;

  ~FormSaverImpl() override;

  // FormSaver:
  PasswordForm Blocklist(PasswordFormDigest digest) override;
  void Unblocklist(const PasswordFormDigest& digest) override;
  void Save(PasswordForm pending,
            const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
                matches,
            const std::u16string& old_password) override;
  void Update(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password) override;
  void UpdateReplace(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password,
      const PasswordForm& old_unique_key) override;
  void Remove(const PasswordForm& form) override;
  std::unique_ptr<FormSaver> Clone() override;

 private:
  // The class is stateless. Don't introduce it. The methods are utilities for
  // common tasks on the password store. The state should belong to either a
  // form handler or origin handler which could embed FormSaver.

  // Cached pointer to the PasswordStoreInterface.
  const raw_ptr<PasswordStoreInterface> store_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_IMPL_H_
