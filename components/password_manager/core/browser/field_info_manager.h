// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordStoreInterface;
struct PasswordForm;

class FieldInfoManager {
 public:
  virtual ~FieldInfoManager() = default;
  virtual void AddFieldType(autofill::FormSignature form_signature,
                            autofill::FieldSignature field_signature,
                            autofill::ServerFieldType field_type) = 0;

  virtual autofill::ServerFieldType GetFieldType(
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature) const = 0;
};

// Keeps semantic types of web forms fields. Fields are specified with a pair
// (FormSignature, FieldSignature), which uniquely defines fields in the web
// (more details on the signature calculation are in signature_util.cc). Types
// might be PASSWORD, USERNAME, NEW_PASSWORD etc.
class FieldInfoManagerImpl : public FieldInfoManager,
                             public KeyedService,
                             public PasswordStoreConsumer {
 public:
  explicit FieldInfoManagerImpl(
      scoped_refptr<password_manager::PasswordStoreInterface> store);
  ~FieldInfoManagerImpl() override;

  // FieldInfoManager:
  void AddFieldType(autofill::FormSignature form_signature,
                    autofill::FieldSignature field_signature,
                    autofill::ServerFieldType field_type) override;
  autofill::ServerFieldType GetFieldType(
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature) const override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetAllFieldInfo(std::vector<FieldInfo>) override;

  std::map<std::pair<autofill::FormSignature, autofill::FieldSignature>,
           autofill::ServerFieldType>
      field_types_;
  scoped_refptr<password_manager::PasswordStoreInterface> store_;

  base::WeakPtrFactory<FieldInfoManagerImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
