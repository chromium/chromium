// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_FIELD_INFO_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_FIELD_INFO_STORE_H_

#include "base/callback.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockFieldInfoStore : public FieldInfoStore {
 public:
  MockFieldInfoStore();
  ~MockFieldInfoStore() override;

  MOCK_METHOD(void, AddFieldInfo, (const FieldInfo& field_info), (override));
  MOCK_METHOD(void,
              GetAllFieldInfo,
              (base::WeakPtr<PasswordStoreConsumer> consumer),
              (override));
  MOCK_METHOD(void,
              RemoveFieldInfoByTime,
              (base::Time remove_begin,
               base::Time remove_end,
               base::OnceClosure completion),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_FIELD_INFO_STORE_H_
