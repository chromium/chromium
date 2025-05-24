// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_ANNOTATED_ACCOUNT_ID_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_ANNOTATED_ACCOUNT_ID_H_

#include "base/component_export.h"
#include "base/supports_user_data.h"
#include "components/account_id/account_id.h"

namespace ash {

// In Ash, most of the profile has corresponding ChromeOS User.
// This class is to support their mapping.
// On Profile creation, AccountId for the corresponding User is annotated.
// See also ProfileUserManagerController.
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) AnnotatedAccountId
    : public base::SupportsUserData::Data {
 public:
  AnnotatedAccountId(const AnnotatedAccountId&) = delete;
  AnnotatedAccountId& operator=(const AnnotatedAccountId&) = delete;
  ~AnnotatedAccountId() override;

  // Returns the AccountId if it is annotated. Otherwise null.
  // Note that some Profiles do not have corresponding AccountId intentionally,
  // such as a Profile for log-in/lock screens.
  // `context` must not be nullptr.
  static const AccountId* Get(base::SupportsUserData* context);

  // Sets the corresponding account id to `context`.
  // `context` must not be nullptr, nor a Profile instance where AccountId is
  // already annotated.
  // `for_test` is a trick to reduce the migration cost of each test.
  // In test code, `AnnotatedAccountId::Set(browser_context, account_id);`
  // is expected to be used (i.e. no explicit specifying of `for_test`).
  static void Set(base::SupportsUserData* context,
                  const AccountId& account_id,
                  bool for_test = true);

 private:
  explicit AnnotatedAccountId(const AccountId& account_id);
  const AccountId account_id_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_ANNOTATED_ACCOUNT_ID_H_
