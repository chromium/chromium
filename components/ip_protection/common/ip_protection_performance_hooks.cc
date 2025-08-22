// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_performance_hooks.h"

#include "base/trace_event/trace_event.h"

namespace ip_protection {

void IpProtectionPerformanceHooks::OnGetInitialDataStart() {
  TRACE_EVENT_BEGIN("ip_protection", "GetInitialData", track_);
}

void IpProtectionPerformanceHooks::OnGetInitialDataEnd() {
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnGenerateBlindedTokenRequestsStart() {
  TRACE_EVENT_BEGIN("ip_protection", "GenerateBlindedTokenRequests", track_);
}

void IpProtectionPerformanceHooks::OnGenerateBlindedTokenRequestsEnd() {
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnAuthAndSignStart() {
  TRACE_EVENT_BEGIN("ip_protection", "AuthAndSign", track_);
}

void IpProtectionPerformanceHooks::OnAuthAndSignEnd() {
  TRACE_EVENT_END("ip_protection", track_);
}

void IpProtectionPerformanceHooks::OnUnblindTokensStart() {
  TRACE_EVENT_BEGIN("ip_protection", "UnblindTokens", track_);
}

void IpProtectionPerformanceHooks::OnUnblindTokensEnd() {
  TRACE_EVENT_END("ip_protection", track_);
}

}  // namespace ip_protection
