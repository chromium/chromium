// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/form_saver.h"

namespace password_manager {

// The no-op FormSaver stub to support tests.
class StubFormSaver : public FormSaver {
 public:
  StubFormSaver() = default;

  StubFormSaver(const StubFormSaver&) = delete;
  StubFormSaver& operator=(const StubFormSaver&) = delete;

  ~StubFormSaver() override = default;

  // FormSaver:
  PasswordForm Blocklist(PasswordFormDigest digest) override;
  void Unblocklist(const PasswordFormDigest& digest) override;
  void Save(PasswordForm pending,
            const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
                matches,
            const std::u16string& old_password) override {}
  void Update(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password) override {}
  void UpdateReplace(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password,
      const PasswordForm& old_unique_key) override {}
  void Remove(const PasswordForm& form) override {}
  std::unique_ptr<FormSaver> Clone() override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_FORM_SAVER_H_
