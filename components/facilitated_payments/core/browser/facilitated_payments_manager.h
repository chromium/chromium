// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"

class GURL;

namespace payments::facilitated {

class FacilitatedPaymentsDriver;

// A cross-platform interface that manages the flow of PIX payments between the
// browser and the Payments platform. It is owned by
// `FacilitatedPaymentsDriver`.
class FacilitatedPaymentsManager {
 public:
  FacilitatedPaymentsManager(
      FacilitatedPaymentsDriver* driver,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  FacilitatedPaymentsManager(const FacilitatedPaymentsManager&) = delete;
  FacilitatedPaymentsManager& operator=(const FacilitatedPaymentsManager&) =
      delete;
  virtual ~FacilitatedPaymentsManager();

  // Initiates the PIX payments flow on the browser. It is invoked by the
  // `FacilitatedPaymentsDriver` when the primary main frame has finished
  // loading.
  void DidFinishLoad(const GURL& url) const;

 private:
  friend class FacilitatedPaymentsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TestRegisterPixOptimizationGuide);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TestShouldDetectPixCode_UrlInAllowlist);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsManagerTest,
                           TestShouldDetectPixCode_UrlNotInAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TestDidFinishLoad_UrlInAllowlist_PixCodeDetectionTriggered);
  FRIEND_TEST_ALL_PREFIXES(
      FacilitatedPaymentsManagerTest,
      TestDidFinishLoad_UrlNotInAllowlist_PixCodeDetectionNotTriggered);

  // Register optimization guide deciders for PIX. It is an allowlist of URLs
  // where we attempt PIX code detection.
  void RegisterPixOptimizationGuide() const;

  // Returns whether PIX detection should be run on the page by querying the PIX
  // allowlist. `url` is the page URL.
  bool ShouldDetectPixCode(const GURL& url) const;

  // Callback to be called after attempting PIX code detection. `pix_code_found`
  // informs whether or not PIX code was found on the page.
  void ProcessPixCodeDetectionResult(bool pix_code_found) const;

  raw_ref<FacilitatedPaymentsDriver> driver_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  base::WeakPtrFactory<FacilitatedPaymentsManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_MANAGER_H_
