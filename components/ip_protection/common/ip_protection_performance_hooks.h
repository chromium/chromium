// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PERFORMANCE_HOOKS_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PERFORMANCE_HOOKS_H_

#include "base/timer/elapsed_timer.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_tracing_hooks.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace ip_protection {

// An implementation of `BlindSignTracingHooks` that emits Perfetto trace
// events and UMA histograms.
class IpProtectionPerformanceHooks : public quiche::BlindSignTracingHooks {
 public:
  explicit IpProtectionPerformanceHooks(perfetto::Track track)
      : track_(track) {}

  // `BlindSignTracingHooks` implementation.
  void OnGetInitialDataStart() override;
  void OnGetInitialDataEnd() override;
  void OnGenerateBlindedTokenRequestsStart() override;
  void OnGenerateBlindedTokenRequestsEnd() override;
  void OnAuthAndSignStart() override;
  void OnAuthAndSignEnd() override;
  void OnUnblindTokensStart() override;
  void OnUnblindTokensEnd() override;

 private:
  perfetto::Track track_;
  base::ElapsedTimer current_phase_timer_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PERFORMANCE_HOOKS_H_
