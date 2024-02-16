// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace autofill {

class ContentAutofillClient;

namespace payments {

// Desktop implementation of the PaymentsWindowManager interface. One per
// WebContents, owned by the ChromeAutofillClient associated with the
// WebContents of the original tab that the pop-up is created in. If there is a
// pop-up currently present, `this` will observe the WebContents of that pop-up.
class DesktopPaymentsWindowManager : public PaymentsWindowManager,
                                     public content::WebContentsObserver {
 public:
  explicit DesktopPaymentsWindowManager(ContentAutofillClient* client);
  DesktopPaymentsWindowManager(const DesktopPaymentsWindowManager&) = delete;
  DesktopPaymentsWindowManager& operator=(const DesktopPaymentsWindowManager&) =
      delete;
  ~DesktopPaymentsWindowManager() override;

  // PaymentsWindowManager:
  void InitVcn3dsAuthentication(Vcn3dsContext context) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  friend class DesktopPaymentsWindowManagerTestApi;

  // Contains the possible flows that this class can support.
  enum class FlowType {
    kNoFlow = 0,
    kVcn3ds = 1,
    kMaxValue = kVcn3ds,
  };

  // Creates a pop-up for `flow_type_`, with an initial URL of `url`. This
  // pop-up will go through a couple of URL navigations specific to the flow
  // that it is created for.
  void CreatePopup(const GURL& url);

  // Triggered when a pop-up is destroyed, and the `flow_type_` is kVcn3ds.
  void OnWebContentsDestroyedForVcn3ds();

  // This callback initiates the second UnmaskCardRequest in the VCN 3DS flow to
  // attempt to retrieve the virtual card. It is run once risk data is loaded
  // for VCN 3DS.
  void OnDidLoadRiskDataForVcn3ds(
      RedirectCompletionProof redirect_completion_proof,
      const std::string& risk_data);

  // Only present if `flow_type_` is `kVcn3ds`.
  std::optional<Vcn3dsContext> vcn_3ds_context_;

  // The type of flow that is currently ongoing. Set when a flow is initiated.
  FlowType flow_type_ = FlowType::kNoFlow;

  // ContentAutofillClient that owns `this`.
  const raw_ref<ContentAutofillClient> client_;

  base::WeakPtrFactory<DesktopPaymentsWindowManager> weak_ptr_factory_{this};
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
