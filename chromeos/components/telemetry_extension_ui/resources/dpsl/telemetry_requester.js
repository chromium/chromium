// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Telemetry interface exposed to third-parties for getting device telemetry
 * information.
 */

(() => {
  const messagePipe = chromeos.internal.messagePipe;

  /**
   * DPSL Telemetry Requester.
   * @suppress {checkTypes}
   */
  class TelemetryRequester extends EventTarget {
    constructor() {
      super();
      messagePipe.registerHandler(
        dpsl_internal.Message.SYSTEM_EVENTS_SERVICE_EVENTS, (message) => {
          const event = /** @type {!dpsl_internal.Event} */ (message);
          this.dispatchEvent(new Event(event.type));
        });
    }

    /**
     * Requests telemetry info.
     * @param { !Array<!string> } categories
     * @return { !Object }
     * @public
     */
    async probeTelemetryInfo(categories) {
      const response =
          /** @type {dpsl_internal.ProbeTelemetryInfoResponse} */ (
          await messagePipe.sendMessage(
            dpsl_internal.Message.PROBE_TELEMETRY_INFO, categories));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }
  };

  globalThis.chromeos.telemetry = new TelemetryRequester();
})();
