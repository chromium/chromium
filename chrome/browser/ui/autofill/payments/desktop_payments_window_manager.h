// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"

class GURL;

namespace autofill {

class AutofillClient;

namespace payments {

// Desktop implementation of the PaymentsWindowManager interface. One per
// WebContents, owned by the ChromeAutofillClient associated with the
// WebContents of the original tab that the pop-up is created in.
class DesktopPaymentsWindowManager : public PaymentsWindowManager {
 public:
  explicit DesktopPaymentsWindowManager(AutofillClient* client);
  DesktopPaymentsWindowManager(const DesktopPaymentsWindowManager&) = delete;
  DesktopPaymentsWindowManager& operator=(const DesktopPaymentsWindowManager&) =
      delete;
  ~DesktopPaymentsWindowManager() override;

  // PaymentsWindowManager:
  void InitVcn3dsAuthentication(Vcn3dsContext context) override;

 private:
  friend class DesktopPaymentsWindowManagerTestApi;

  // Contains the possible flows that this class can support.
  enum class FlowType {
    kNoFlow = 0,
    kVcn3ds = 1,
    kMaxValue = kVcn3ds,
  };

  void CreatePopup(const GURL& url);

  // Only present if `flow_type_` is `kVcn3ds`.
  std::optional<Vcn3dsContext> vcn_3ds_context_;

  // The type of flow that is currently ongoing. Set when a flow is initiated.
  FlowType flow_type_ = FlowType::kNoFlow;

  // AutofillClient that owns `this`.
  const raw_ref<AutofillClient> client_;
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
