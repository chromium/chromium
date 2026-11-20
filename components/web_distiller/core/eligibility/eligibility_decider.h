// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_ELIGIBILITY_DECIDER_H_
#define COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_ELIGIBILITY_DECIDER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/web_distiller/core/eligibility/reader_mode_eligibility.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace web_distiller {

// Maps the value returned by the triggering script. A bool maps to eligible or
// not eligible. Anything else, including null, is `kMalformedResponse`.
ReaderModeEligibility EligibilityFromScriptValue(const base::Value* value);

// Maps the READER_MODE_ELIGIBLE decision. `kUnknown` counts as ineligible.
ReaderModeEligibility EligibilityFromOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision);

// Returns true if `url` can run the eligibility check: http(s), and not a
// Google home, search, YouTube, or known Workspace page.
bool IsUrlEligible(const GURL& url);

// Decides whether a page is eligible for Reading Mode. After `start_delay`,
// checks the URL and content type, runs the Readability triggering script and,
// only if the script says eligible, asks the Optimization Guide for
// READER_MODE_ELIGIBLE. UI thread only.
class EligibilityDecider {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual GURL GetLastCommittedURL() const = 0;
    virtual bool IsContentHTML() const = 0;
    // Runs `script` in the main frame and passes its result to `callback`. If
    // there's no frame, drop `callback` without running it; the decision then
    // ends without a result.
    virtual void ExecuteScript(
        const std::string& script,
        base::OnceCallback<void(const base::Value*)> callback) = 0;
  };

  // `result` is nullopt when the page failed the URL or content check, so no
  // script ran. A decision whose final stage is already in flight reports its
  // result with `is_current_url` false when the committed URL changed or a
  // newer decision or Cancel() superseded it; if canceled or URL-changed
  // before `ExecuteScript` or before a required `CanApplyOptimization` call
  // starts, the incomplete decision ends without reporting.
  using ResultCallback =
      base::RepeatingCallback<void(const GURL& url,
                                   std::optional<ReaderModeEligibility> result,
                                   bool is_current_url)>;

  // `optimization_guide_decider` may be null, in which case the Readability
  // script alone decides. If non-null, it must already have
  // READER_MODE_ELIGIBLE registered. `delegate` and
  // `optimization_guide_decider` must outlive this object.
  EligibilityDecider(
      Delegate& delegate,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      base::TimeDelta start_delay,
      ResultCallback on_result);
  EligibilityDecider(const EligibilityDecider&) = delete;
  EligibilityDecider& operator=(const EligibilityDecider&) = delete;
  ~EligibilityDecider();

  // Cancels any pending or in-flight decision, then starts a new one for `url`.
  void Start(const GURL& url);

  // Returns true if a pending (pre-script) decision was canceled. A decision
  // whose final stage is already in flight still reports its result with
  // `is_current_url` false; if canceled before `ExecuteScript` or before a
  // required `CanApplyOptimization` call starts, the incomplete decision ends
  // without reporting.
  bool Cancel();

 private:
  void EvaluateReadability(const GURL& url, int decision_id);
  void OnScriptResult(const GURL& url,
                      int decision_id,
                      const base::Value* value);
  void OnOptimizationGuideDecision(
      const GURL& url,
      int decision_id,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);
  void ReportResult(const GURL& url,
                    int decision_id,
                    std::optional<ReaderModeEligibility> result);
  bool IsCurrentDecision(int decision_id) const;

  const raw_ref<Delegate> delegate_;
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const base::TimeDelta start_delay_;
  ResultCallback on_result_;
  base::OneShotTimer start_timer_;
  // Incremented on every Start() and Cancel() so in-flight results from an
  // older decision can be told apart.
  int current_decision_id_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EligibilityDecider> weak_ptr_factory_{this};
};

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_ELIGIBILITY_DECIDER_H_
