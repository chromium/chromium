// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NetworkDiagnosticsRoutinesInterface, RoutineResult, RoutineType} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
import {RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

import {createResult} from './network_health_test_utils.js';

export interface RunRoutineResponse {
  result: RoutineResult;
}

export class FakeNetworkDiagnostics implements
    NetworkDiagnosticsRoutinesInterface {
  private verdict_: RoutineVerdict = RoutineVerdict.kNoProblem;
  private problem_: number|null = null;
  private resolvers_: Function[] = [];

  /**
   * Sets the RoutineVerdict to be used by all routines in the
   * FakeNetworkDiagnostics service. Problems will be added automatically if the
   * verdict is kProblem.
   */
  setFakeVerdict(verdict: RoutineVerdict): void {
    this.verdict_ = verdict;
    if (verdict === RoutineVerdict.kProblem) {
      this.problem_ = 0;
    }
  }

  /**
   * Resolves the pending promises of the network diagnostics routine responses.
   */
  resolveRoutines(): void {
    this.resolvers_.map(resolver => resolver());
    this.resolvers_ = [];
  }

  /**
   * Wraps a RoutineResult structure in a form that is expected by mojo methods
   * and adds the promise resolver to the resolvers list.
   */
  private wrapResult_(problemField: string): Promise<RunRoutineResponse> {
    const result = createResult(this.verdict_);
    const problems = new Map<string, number[]>();
    problems.set(problemField, this.problem_ !== null ? [this.problem_] : []);
    result.problems = Object.assign(result.problems, problems);
    const response = {
      result: result,
    };

    let resolver: Function;
    const promise: Promise<RunRoutineResponse> = new Promise((resolve) => {
      resolver = resolve;
    });
    this.resolvers_.push(() => {
      resolver(response);
    });
    return promise;
  }

  runLanConnectivity(): Promise<RunRoutineResponse> {
    return this.wrapResult_('lanConnectivityProblems');
  }

  runSignalStrength(): Promise<RunRoutineResponse> {
    return this.wrapResult_('signalStrengthProblems');
  }

  runGatewayCanBePinged(): Promise<RunRoutineResponse> {
    return this.wrapResult_('gatewayCanBePingedProblems');
  }

  runHasSecureWiFiConnection(): Promise<RunRoutineResponse> {
    return this.wrapResult_('hasSecureWifiConnectionProblems');
  }

  runDnsResolverPresent(): Promise<RunRoutineResponse> {
    return this.wrapResult_('dnsResolverPresentProblems');
  }

  runDnsLatency(): Promise<RunRoutineResponse> {
    return this.wrapResult_('dnsLatencyProblems');
  }

  runDnsResolution(): Promise<RunRoutineResponse> {
    return this.wrapResult_('dnsResolutionProblems');
  }

  runCaptivePortal(): Promise<RunRoutineResponse> {
    return this.wrapResult_('captivePortalProblems');
  }

  runHttpFirewall(): Promise<RunRoutineResponse> {
    return this.wrapResult_('httpFirewallProblems');
  }

  runHttpsFirewall(): Promise<RunRoutineResponse> {
    return this.wrapResult_('httpsFirewallProblems');
  }

  runHttpsLatency(): Promise<RunRoutineResponse> {
    return this.wrapResult_('httpsLatencyProblems');
  }

  runVideoConferencing(_: string|null): Promise<RunRoutineResponse> {
    return this.wrapResult_('videoConferencingProblems');
  }

  runArcHttp(): Promise<RunRoutineResponse> {
    return this.wrapResult_('arcHttpProblems');
  }

  runArcDnsResolution(): Promise<RunRoutineResponse> {
    return this.wrapResult_('arcDnsResolutionProblems');
  }

  runArcPing(): Promise<RunRoutineResponse> {
    return this.wrapResult_('arcPingProblems');
  }

  /**
   * NOT IMPLEMENTED: getResult API is not currently used in the UI.
   */
  getResult(_: RoutineType): Promise<{result: RoutineResult | null}> {
    return Promise.resolve({
      result: null,
    });
  }

  /**
   * NOT IMPLEMENTED: getAllResult API is not currently used in the UI.
   */
  getAllResults(): Promise<{results: Map<RoutineType, RoutineResult>}> {
    return Promise.resolve({
      results: new Map(),
    });
  }
}
