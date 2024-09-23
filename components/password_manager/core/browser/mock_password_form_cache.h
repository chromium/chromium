// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_CACHE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_CACHE_H_

#include "components/password_manager/core/browser/password_form_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFormCache : public PasswordFormCache {
 public:
  MockPasswordFormCache();
  ~MockPasswordFormCache() override;
  MockPasswordFormCache(const MockPasswordFormCache&) = delete;
  MockPasswordFormCache& operator=(const MockPasswordFormCache&) = delete;
  MOCK_METHOD(const PasswordForm*,
              GetPasswordForm,
              (PasswordManagerDriver*, autofill::FormRendererId),
              (const override));
  MOCK_METHOD(const PasswordForm*,
              GetPasswordForm,
              (PasswordManagerDriver*, autofill::FieldRendererId),
              (const override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_CACHE_H_
