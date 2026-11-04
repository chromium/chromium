// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_GATEWAY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_GATEWAY_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;

// Tab-scoped gateway owned by `OtpSecurityGateway` that wraps
// `OtpPhishGuardDelegate`, serializes concurrent checks on the tab's single
// delegate instance, and records PhishGuard metrics.
class OtpPhishGuardGateway {
 public:
  static constexpr char kCheckPerformedHistogram[] =
      "Autofill.OneTimeTokens.PhishGuard.CheckPerformed";
  static constexpr char kLatencyHistogram[] =
      "Autofill.OneTimeTokens.PhishGuard.Latency";
  static constexpr char kVerdictHistogram[] =
      "Autofill.OneTimeTokens.PhishGuard.Verdict";

  using VerdictCallback =
      base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)>;

  explicit OtpPhishGuardGateway(AutofillClient& client);
  OtpPhishGuardGateway(const OtpPhishGuardGateway&) = delete;
  OtpPhishGuardGateway& operator=(const OtpPhishGuardGateway&) = delete;
  ~OtpPhishGuardGateway();

  // Performs a PhishGuard check for `frame`, invokes `callback` with the
  // resulting verdict (`kUnknown` when no `OtpPhishGuardDelegate` is attached),
  // and records `CheckPerformed`, `Latency`, and `Verdict` histograms.
  // Concurrent calls are serialized in FIFO order so single-flight delegates
  // such as `ChromeOtpPhishGuardDelegate` never overwrite an in-flight check.
  // Verdicts are intentionally not cached by `LocalFrameToken` because
  // same-`RenderFrameHost` navigations (such as SPA URL changes or
  // `document.open()`) reuse the frame token; verified OTP candidates are
  // cached per field in `OtpDelivery`.
  void Check(LocalFrameToken frame, VerdictCallback callback);

 private:
  enum class DispatchState {
    kIdle,
    kWaitingForVerdict,
    kRunningCallback,
  };

  struct PendingCheck {
    PendingCheck(LocalFrameToken frame, VerdictCallback callback);
    PendingCheck(PendingCheck&&) noexcept;
    PendingCheck& operator=(PendingCheck&&) noexcept;
    ~PendingCheck();

    LocalFrameToken frame;
    VerdictCallback callback;
  };

  void DrainPendingChecks();
  void OnPhishGuardCheckCompleted(base::TimeTicks start_time,
                                  VerdictCallback callback,
                                  bool is_phishing_site);

  const raw_ref<AutofillClient> client_;
  DispatchState dispatch_state_ = DispatchState::kIdle;
  base::queue<PendingCheck> pending_checks_;

  base::WeakPtrFactory<OtpPhishGuardGateway> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_GATEWAY_H_
