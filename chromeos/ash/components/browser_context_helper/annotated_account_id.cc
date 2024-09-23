// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

namespace {
void* AnnotatedAccountIdKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int data_key = 0;
  return &data_key;
}
}  // namespace

AnnotatedAccountId::~AnnotatedAccountId() = default;

// static
const AccountId* AnnotatedAccountId::Get(base::SupportsUserData* context) {
  CHECK(context);
  auto* data = static_cast<AnnotatedAccountId*>(
      context->GetUserData(AnnotatedAccountIdKey()));
  if (!data) {
    return nullptr;
  }
  return &data->account_id_;
}

// static
void AnnotatedAccountId::Set(base::SupportsUserData* context,
                             const AccountId& account_id,
                             bool for_test) {
  CHECK(context);
  CHECK(!context->GetUserData(AnnotatedAccountIdKey()));
  context->SetUserData(AnnotatedAccountIdKey(),
                       base::WrapUnique(new AnnotatedAccountId(account_id)));

  // TODO(b/40225390): On attempting to set AccountId in tests,
  // we forcibly enable mapping based on annotated AccountId
  // in BrowserContextHelper. This is a workaround to reduce the
  // migration cost in call sites. Remove this hack after the migration.
  if (for_test) {
    BrowserContextHelper::Get()
        ->SetUseAnnotatedAccountIdForTesting();  // IN-TEST
  }
}

AnnotatedAccountId::AnnotatedAccountId(const AccountId& account_id)
    : account_id_(account_id) {}

}  // namespace ash
