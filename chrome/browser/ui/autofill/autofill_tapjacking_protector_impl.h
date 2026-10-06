// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"

namespace autofill {

class AutofillClient;

class AutofillTapjackingProtectorImpl : public AutofillTapjackingProtector {
 public:
  explicit AutofillTapjackingProtectorImpl(AutofillClient* autofill_client);

  AutofillTapjackingProtectorImpl(const AutofillTapjackingProtectorImpl&) =
      delete;
  AutofillTapjackingProtectorImpl& operator=(
      const AutofillTapjackingProtectorImpl&) = delete;
  ~AutofillTapjackingProtectorImpl() override;

  // AutofillTapjackingProtector:
  void Show(AuthorizationType authorization_type,
            AuthorizationCallback callback) override;

 private:
  const raw_ref<AutofillClient> autofill_client_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_
