// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill::payments {

// Chrome implementation of PaymentsAutofillClient. Used for Chrome Desktop and
// Clank. Owned by the ChromeAutofillClient. Created lazily in the
// ChromeAutofillClient when it is needed, and it observes the same WebContents
// as its owning ChromeAutofillClient.
class ChromePaymentsAutofillClient : public PaymentsAutofillClient,
                                     public content::WebContentsObserver {
 public:
  explicit ChromePaymentsAutofillClient(content::WebContents* web_contents);
  ChromePaymentsAutofillClient(const ChromePaymentsAutofillClient&) = delete;
  ChromePaymentsAutofillClient& operator=(const ChromePaymentsAutofillClient&) =
      delete;
  ~ChromePaymentsAutofillClient() override;

#if !BUILDFLAG(IS_ANDROID)
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
