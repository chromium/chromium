// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_

#include <map>

#include "components/autofill/core/browser/field_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordStore;

// Keeps semantic types of web forms fields. Fields are specified with a pair
// (FormSignature, FieldSignature), which uniquely defines fields in the web
// (more details on the signature calculation are in signature_util.cc). Types
// might be PASSWORD, USERNAME, NEW_PASSWORD etc.
class FieldInfoManager : public KeyedService, public PasswordStoreConsumer {
 public:
  FieldInfoManager(scoped_refptr<password_manager::PasswordStore> store);
  ~FieldInfoManager() override;

  void AddFieldType(uint64_t form_signature,
                    uint32_t field_signature,
                    autofill::ServerFieldType field_type);
  autofill::ServerFieldType GetFieldType(uint64_t form_signature,
                                         uint32_t field_signature) const;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;
  void OnGetAllFieldInfo(std::vector<FieldInfo>) override;

  std::map<std::pair<uint64_t, uint32_t>, autofill::ServerFieldType>
      field_types_;
  scoped_refptr<password_manager::PasswordStore> store_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
