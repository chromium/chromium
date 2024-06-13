// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/fast_checkout_enums.h"

class GURL;

namespace autofill {

class AutofillManager;
class FormData;
class FormFieldData;

// Abstract interface for handling a fast checkout run.
class FastCheckoutClient {
 public:
  FastCheckoutClient(const FastCheckoutClient&) = delete;
  FastCheckoutClient& operator=(const FastCheckoutClient&) = delete;
  virtual ~FastCheckoutClient() = default;

  // Starts the fast checkout run. Returns true if the run was successful.
  virtual bool TryToStart(
      const GURL& url,
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      base::WeakPtr<autofill::AutofillManager> autofill_manager) = 0;

  // Stops the fast checkout run. Resets internal UI state to `kNotShownYet` if
  // `allow_further_runs == true`.
  virtual void Stop(bool allow_further_runs) = 0;

  // Returns true if a fast checkout run is ongoing.
  virtual bool IsRunning() const = 0;

  // Returns true if the bottomsheet is currently showing to the user.
  virtual bool IsShowing() const = 0;

  // Notifies the `FastCheckoutClient` when a navigation happened.
  virtual void OnNavigation(const GURL& url, bool is_cart_or_checkout_url) = 0;

  // Returns the outcome of trying to launch FC on `form` and `field`.
  virtual autofill::FastCheckoutTriggerOutcome CanRun(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const autofill::AutofillManager& autofill_manager) const = 0;

  virtual bool IsNotShownYet() const = 0;

 protected:
  FastCheckoutClient() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_CLIENT_H_
