// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"

class GURL;

namespace autofill {

class ContentAutofillClient;

namespace payments {

class FlowState;

// Android implementation of the PaymentsWindowManager interface. One per
// WebContents, owned by the ChromePaymentsAutofillClient associated with the
// WebContents of the original tab that the tab is created in.
class AndroidPaymentsWindowManager : public PaymentsWindowManager {
 public:
  explicit AndroidPaymentsWindowManager(ContentAutofillClient* client);
  AndroidPaymentsWindowManager(const AndroidPaymentsWindowManager&) = delete;
  AndroidPaymentsWindowManager& operator=(const AndroidPaymentsWindowManager&) =
      delete;
  ~AndroidPaymentsWindowManager() override;

  // PaymentsWindowManager:
  void InitBnplFlow(BnplContext context) override;
  void InitVcn3dsAuthentication(Vcn3dsContext context) override;

  // Triggered when the web contents of a tab shown as part of a window manager
  // flow was destroyed.
  void WebContentsDestroyed();

  // Triggered when a tab navigation has finished, and `flow_state_->flow_type`
  // is `kBnpl`.
  void OnDidFinishNavigationForBnpl(const GURL& url);

 private:
  friend class AndroidPaymentsWindowManagerTestApi;

  // Creates a tab using `flow_state_`, with an initial URL of `url`. This tab
  // will go through a couple of URL navigations specific to the flow that it is
  // created for.
  void CreateTab(const GURL& url);

  // Keeps track of the state for the ongoing flow. Present only if there is an
  // ongoing flow, and is empty otherwise.
  std::optional<FlowState> flow_state_;

  // ContentAutofillClient associated to `this`.
  const raw_ref<ContentAutofillClient> client_;

  base::WeakPtrFactory<AndroidPaymentsWindowManager> weak_ptr_factory_{this};
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_H_
