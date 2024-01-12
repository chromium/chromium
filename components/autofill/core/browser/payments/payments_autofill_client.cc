// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include "base/functional/callback.h"

namespace autofill::payments {

PaymentsAutofillClient::~PaymentsAutofillClient() = default;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void PaymentsAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace autofill::payments
