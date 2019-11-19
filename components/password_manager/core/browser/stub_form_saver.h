// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/form_saver.h"

namespace password_manager {

// The no-op FormSaver stub to support tests.
class StubFormSaver : public FormSaver {
 public:
  StubFormSaver() = default;

  ~StubFormSaver() override = default;

  // FormSaver:
  autofill::PasswordForm PermanentlyBlacklist(
      PasswordStore::FormDigest digest) override;
  void Unblacklist(const PasswordStore::FormDigest& digest) override;
  void Save(autofill::PasswordForm pending,
            const std::vector<const autofill::PasswordForm*>& matches,
            const base::string16& old_password) override {}
  void Update(autofill::PasswordForm pending,
              const std::vector<const autofill::PasswordForm*>& matches,
              const base::string16& old_password) override {}
  void UpdateReplace(autofill::PasswordForm pending,
                     const std::vector<const autofill::PasswordForm*>& matches,
                     const base::string16& old_password,
                     const autofill::PasswordForm& old_unique_key) override {}
  void Remove(const autofill::PasswordForm& form) override {}
  std::unique_ptr<FormSaver> Clone() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubFormSaver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_
