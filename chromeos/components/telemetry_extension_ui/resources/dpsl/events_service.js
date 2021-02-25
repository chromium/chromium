// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Events service interface exposed to third-parties to (un)subscribe on
 * different system event types.
 */

(() => {

  /**
   * DPSL Events service for dpsl.system_events.* APIs.
   */
  class DPSLEventsService {

    /**
     * Returns the list of supported events.
     * @returns {!dpsl.EventTypes}
     * @public
     */
    getAvailableEvents() {
      return /** @type {dpsl.EventTypes} */ ([
        'ac-inserted', 'ac-removed', 'bluetooth-adapter-added',
        'bluetooth-adapter-property-changed', 'bluetooth-adapter-removed',
        'bluetooth-device-added', 'bluetooth-device-property-changed',
        'bluetooth-device-removed', 'lid-closed', 'lid-opened', 'os-resume',
        'os-suspend'
      ]);
    }
  }

  globalThis.dpsl.system_events = new DPSLEventsService();
})();
