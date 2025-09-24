// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_performance_hooks.h"

#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"

namespace ip_protection {

void IpProtectionPerformanceHooks::OnGetInitialDataStart() {
  current_phase_timer_ = base::ElapsedTimer();
  TRACE_EVENT_BEGIN("ip_protection", "GetInitialData", track_);
}

void IpProtectionPerformanceHooks::OnGetInitialDataEnd() {
  Telemetry().TokenBatchGenerationPhaseTime(BlindSignAuthPhase::kGetInitialData,
                                            current_phase_timer_.Elapsed());
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnGenerateBlindedTokenRequestsStart() {
  current_phase_timer_ = base::ElapsedTimer();
  TRACE_EVENT_BEGIN("ip_protection", "GenerateBlindedTokenRequests", track_);
}

void IpProtectionPerformanceHooks::OnGenerateBlindedTokenRequestsEnd() {
  Telemetry().TokenBatchGenerationPhaseTime(
      BlindSignAuthPhase::kGenerateBlindedTokenRequests,
      current_phase_timer_.Elapsed());
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnAuthAndSignStart() {
  current_phase_timer_ = base::ElapsedTimer();
  TRACE_EVENT_BEGIN("ip_protection", "AuthAndSign", track_);
}

void IpProtectionPerformanceHooks::OnAuthAndSignEnd() {
  Telemetry().TokenBatchGenerationPhaseTime(BlindSignAuthPhase::kAuthAndSign,
                                            current_phase_timer_.Elapsed());
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnUnblindTokensStart() {
  current_phase_timer_ = base::ElapsedTimer();
  TRACE_EVENT_BEGIN("ip_protection", "UnblindTokens", track_);
}

void IpProtectionPerformanceHooks::OnUnblindTokensEnd() {
  Telemetry().TokenBatchGenerationPhaseTime(BlindSignAuthPhase::kUnblindTokens,
                                            current_phase_timer_.Elapsed());
  TRACE_EVENT_END("ip_protection", track_);
}

}  // namespace ip_protection
