// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkDiagnosticsRoutinesInterface, RoutineResult, RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

import {assertNotReached} from '../../../chromeos/chai_assert.js';

import {createResult} from './network_health_test_utils.js';

/**
 * @typedef {{
 *            result: !RoutineResult,
 *          }}
 */
let RunRoutineResponse;

/**
 * @implements {NetworkDiagnosticsRoutinesInterface}
 */
export class FakeNetworkDiagnostics {
  constructor() {
    /** @private {!RoutineVerdict} */
    this.verdict_ = RoutineVerdict.kNoProblem;

    /** @private {?number} */
    this.problem_ = null;

    /** @private {!Array<Function>} */
    this.resolvers_ = [];
  }

  /**
   * Sets the RoutineVerdict to be used by all routines in the
   * FakeNetworkDiagnostics service. Problems will be added automatically if the
   * verdict is kProblem.
   * @param {!RoutineVerdict} verdict
   */
  setFakeVerdict(verdict) {
    this.verdict_ = verdict;
    if (verdict === RoutineVerdict.kProblem) {
      this.problem_ = 0;
    }
  }

  /**
   * Resolves the pending promises of the network diagnostics routine responses.
   */
  resolveRoutines() {
    this.resolvers_.map(resolver => resolver());
    this.resolvers_ = [];
  }

  /**
   * Wraps a RoutineResult structure in a form that is expected by mojo methods
   * and adds the promise resolver to the resolvers list.
   * @private
   * @param {string} problemField
   * @returns {!Promise<!RunRoutineResponse>}
   */
  wrapResult_(problemField) {
    const result = createResult(this.verdict_);
    result.problems[problemField] =
        this.problem_ !== null ? [this.problem_] : [];
    const response = {
      result: result,
    };

    let resolver;
    const promise = new Promise((resolve) => {
      resolver = resolve;
    });
    this.resolvers_.push(() => {
      resolver(response);
    });
    return promise;
  }

  /** @override */
  runLanConnectivity() {
    return this.wrapResult_('lanConnectivityProblems');
  }

  /** @override */
  runSignalStrength() {
    return this.wrapResult_('signalStrengthProblems');
  }

  /** @override */
  runGatewayCanBePinged() {
    return this.wrapResult_('gatewayCanBePingedProblems');
  }

  /** @override */
  runHasSecureWiFiConnection() {
    return this.wrapResult_('hasSecureWifiConnectionProblems');
  }

  /** @override */
  runDnsResolverPresent() {
    return this.wrapResult_('dnsResolverPresentProblems');
  }

  /** @override */
  runDnsLatency() {
    return this.wrapResult_('dnsLatencyProblems');
  }

  /** @override */
  runDnsResolution() {
    return this.wrapResult_('dnsResolutionProblems');
  }

  /** @override */
  runCaptivePortal() {
    return this.wrapResult_('captivePortalProblems');
  }

  /** @override */
  runHttpFirewall() {
    return this.wrapResult_('httpFirewallProblems');
  }

  /** @override */
  runHttpsFirewall() {
    return this.wrapResult_('httpsFirewallProblems');
  }

  /** @override */
  runHttpsLatency() {
    return this.wrapResult_('httpsLatencyProblems');
  }

  /** @override */
  runVideoConferencing(stun_server_hostname) {
    return this.wrapResult_('videoConferencingProblems');
  }

  /** @override */
  runArcHttp() {
    return this.wrapResult_('arcHttpProblems');
  }

  /** @override */
  runArcDnsResolution() {
    return this.wrapResult_('arcDnsResolutionProblems');
  }

  /** @override */
  runArcPing() {
    return this.wrapResult_('arcPingProblems');
  }

  /**
   * NOT IMPLEMNTED: getResult API is not currently used in the UI.
   * @override
   */
  getResult(routine) {
    assertNotReached();
    return Promise.resolve({
      result: null,
    });
  }

  /**
   * NOT IMPLEMNTED: getAllResult API is not currently used in the UI.
   * @override
   */
  getAllResults() {
    assertNotReached();
    return Promise.resolve({
      results: new Map(),
    });
  }
}
