// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

namespace autofill {

class ContentAutofillClient;

namespace payments {

class PaymentsWindowUserConsentDialogControllerImpl;

// Desktop implementation of the PaymentsWindowManager interface. One per
// WebContents, owned by the ChromeAutofillClient associated with the
// WebContents of the original tab that the pop-up is created in. If there is a
// pop-up currently present, `this` will observe the WebContents of that pop-up.
class DesktopPaymentsWindowManager : public PaymentsWindowManager,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
                                     public BrowserListObserver,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  friend class DesktopPaymentsWindowManagerTestApi;

  // Contains the possible flows that this class can support.
  enum class FlowType {
    kNoFlow = 0,
    kVcn3ds = 1,
    kMaxValue = kVcn3ds,
  };

  // Creates a pop-up for `flow_type_`, with an initial URL of `url` and size of
  // `popup_size`. This pop-up will go through a couple of URL navigations
  // specific to the flow that it is created for.
  void CreatePopup(const GURL& url, gfx::Rect popup_size);

  // Triggered when a pop-up navigation has finished, and the `flow_type_` is
  // kVcn3ds.
  void OnDidFinishNavigationForVcn3ds();

  // Triggered when a pop-up is destroyed, and the `flow_type_` is kVcn3ds.
  void OnWebContentsDestroyedForVcn3ds();

  // Initiates the second UnmaskCardRequest in the VCN 3DS flow to attempt to
  // retrieve the virtual card. This method is run once risk data is loaded for
  // VCN 3DS.
  void OnDidLoadRiskDataForVcn3ds(
      RedirectCompletionResult redirect_completion_result,
      const std::string& risk_data);

  // Closes the progress dialog and runs the completion callback
  // `vcn_3ds_context_`. Run once a response is received from the second
  // UnmaskCardRequest, triggered after the authentication has completed.
  void OnVcn3dsAuthenticationResponseReceived(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const PaymentsNetworkInterface::UnmaskResponseDetails& response_details);

  // Resets the state of `this` in relation to the ongoing UnmaskCardRequest.
  // Called if the user clicks cancel on the progress dialog, which is shown
  // after a pop-up with valid query params gets closed and the second
  // UnmaskCardRequest is triggered.
  void OnVcn3dsAuthenticationProgressDialogCancelled();

  // Shows the VCN 3DS consent dialog, which the user must accept for the pop-up
  // window to trigger. If the user cancels the dialog the flow will end.
  void ShowVcn3dsConsentDialog();

  // Handles the user accepting the VCN 3DS consent dialog.
  void OnVcn3dsConsentDialogAccepted();

  // Handles the user cancelling the VCN 3DS consent dialog.
  void OnVcn3dsConsentDialogCancelled();

  // Resets the state of `this` in relation to the ongoing flow.
  void Reset();

  // Only present if `flow_type_` is `kVcn3ds`.
  std::optional<Vcn3dsContext> vcn_3ds_context_;

  // The timestamp for when the VCN 3DS pop-up was shown to the user. Used for
  // logging purposes.
  std::optional<base::TimeTicks> vcn_3ds_popup_shown_timestamp_;

  // The type of flow that is currently ongoing. Set when a flow is initiated.
  FlowType flow_type_ = FlowType::kNoFlow;

  // ContentAutofillClient that owns `this`.
  const raw_ref<ContentAutofillClient> client_;

  // Controller for the VCN 3DS consent dialog. Set (and re-set if it was
  // previously set) when the dialog is triggered.
  std::unique_ptr<PaymentsWindowUserConsentDialogControllerImpl>
      payments_window_user_consent_dialog_controller_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  base::ScopedObservation<BrowserList, BrowserListObserver> scoped_observation_{
      this};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

  base::WeakPtrFactory<DesktopPaymentsWindowManager> weak_ptr_factory_{this};
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
