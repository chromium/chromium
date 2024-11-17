// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineCallSource} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
import type {RoutineResult, RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
/**
 * Removes any prefixed URL from a icon image path
 */
export function getIconFromSrc(src: string): string {
  const values: string[] = src.split('/');
  return values[values.length - 1] || '';
}

/**
 * Creates and returns a basic RoutineResult structure
 */
export function createResult(verdict: RoutineVerdict): RoutineResult {
  return {
    source: RoutineCallSource.kChromeNetworkPage,
    verdict: verdict,
    problems: {
      lanConnectivityProblems: undefined,
      signalStrengthProblems: undefined,
      gatewayCanBePingedProblems: undefined,
      hasSecureWifiConnectionProblems: undefined,
      dnsResolverPresentProblems: undefined,
      dnsLatencyProblems: undefined,
      dnsResolutionProblems: undefined,
      captivePortalProblems: undefined,
      httpFirewallProblems: undefined,
      httpsFirewallProblems: undefined,
      httpsLatencyProblems: undefined,
      videoConferencingProblems: undefined,
      arcHttpProblems: undefined,
      arcDnsResolutionProblems: undefined,
      arcPingProblems: undefined,
    },
    timestamp: {
      internalValue: BigInt(0),
    },
    resultValue: undefined,
  };
}
