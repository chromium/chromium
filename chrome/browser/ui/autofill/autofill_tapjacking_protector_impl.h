// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"

namespace autofill {

class AutofillDialogController;

class AutofillTapjackingProtectorImpl : public AutofillTapjackingProtector {
 public:
  explicit AutofillTapjackingProtectorImpl(
      AutofillDialogController* autofill_dialog_controller);

  AutofillTapjackingProtectorImpl(const AutofillTapjackingProtectorImpl&) =
      delete;
  AutofillTapjackingProtectorImpl& operator=(
      const AutofillTapjackingProtectorImpl&) = delete;
  ~AutofillTapjackingProtectorImpl() override;

  // AutofillTapjackingProtector:
  void Show(AuthorizationType authorization_type,
            AuthorizationCallback callback) override;

 private:
  std::u16string GetDialogTitle(AuthorizationType authorization_type) const;
  std::u16string GetDialogDescription(
      AuthorizationType authorization_type) const;

  const raw_ptr<AutofillDialogController> autofill_dialog_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_TAPJACKING_PROTECTOR_IMPL_H_
