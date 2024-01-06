// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

namespace autofill::payments {

// A payments-specific client interface that handles dependency injection, and
// its implementations serve as the integration for platform-specific code. One
// per WebContents, owned by the AutofillClient. Created lazily in the
// AutofillClient when it is needed.
class PaymentsAutofillClient {
 public:
  virtual ~PaymentsAutofillClient();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Runs |show_migration_dialog_closure| if the user accepts the card migration
  // offer. This causes the card migration dialog to be shown.
  virtual void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
