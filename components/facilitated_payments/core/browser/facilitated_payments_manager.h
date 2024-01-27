// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

class GURL;

namespace payments::facilitated {

class FacilitatedPaymentsDriver;

// A cross-platform interface that manages the flow of PIX payments between the
// browser and the Payments platform. It is owned by
// `FacilitatedPaymentsDriver`.
class FacilitatedPaymentsManager {
 public:
  explicit FacilitatedPaymentsManager(FacilitatedPaymentsDriver* driver);
  FacilitatedPaymentsManager(const FacilitatedPaymentsManager&) = delete;
  FacilitatedPaymentsManager& operator=(const FacilitatedPaymentsManager&) =
      delete;
  virtual ~FacilitatedPaymentsManager();

  // Initiates the PIX payments flow on the browser. It is invoked by the
  // `FacilitatedPaymentsDriver` when the primary main frame has finished
  // loading.
  void DidFinishLoad(const GURL& url) const;

 private:
  // Returns whether PIX detection should be run on the page by querying the PIX
  // allowlist. `url` is the page URL.
  bool ShouldDetectPixCode(const GURL& url) const;

  // Callback to be called after attempting PIX code detection. `pix_code_found`
  // informs whether or not PIX code was found on the page.
  void ProcessPixCodeDetectionResult(bool pix_code_found) const;

  raw_ref<FacilitatedPaymentsDriver> driver_;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
