// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {GAHelperInterface} from './untrusted_helper_interfaces.js';

/**
 * The GA library URL in trusted type.
 * @type {!TrustedScriptURL}
 */
const gaLibraryURL = (() => {
  const staticUrlPolicy = trustedTypes.createPolicy(
      'ga-js-static', {createScriptURL: () => '../js/lib/analytics.js'});
  return staticUrlPolicy.createScriptURL('');
})();

/**
 * Initializes GA for sending metrics.
 * @param {string} id The GA tracker ID to send metrics.
 * @param {string} clientId The GA client ID representing the current client.
 * @param {function(string): void} setClientIdCallback Callback to store
 *     client id.
 * @return {!Promise}
 */
async function initGA(id, clientId, setClientIdCallback) {
  // GA initialization function which is mostly copied from
  // https://developers.google.com/analytics/devguides/collection/analyticsjs.
  (function(i, s, o, g, r) {
    i['GoogleAnalyticsObject'] = r;
    i[r] = i[r] || function(...args) {
      (i[r].q = i[r].q || []).push(args);
    }, i[r].l = Date.now();
    const a = s.createElement(o);
    const m = s.getElementsByTagName(o)[0];
    a['async'] = 1;
    a['src'] = g;
    m.parentNode.insertBefore(a, m);
  })(window, document, 'script', gaLibraryURL, 'ga');

  window.ga('create', id, {
    'storage': 'none',
    'clientId': clientId,
  });

  window.ga((tracker) => setClientIdCallback(tracker.get('clientId')));

  // By default GA uses a fake image and sets its source to the target URL to
  // record metrics. Since requesting remote image violates the policy of
  // a platform app, use navigator.sendBeacon() instead.
  window.ga('set', 'transport', 'beacon');

  // By default GA only accepts "http://" and "https://" protocol. Bypass the
  // check here since we are "chrome-extension://".
  window.ga('set', 'checkProtocolTask', null);
}

/**
 * Sends event to GA.
 * @param {!UniversalAnalytics.FieldsObject} event Event to send.
 * @return {!Promise}
 */
async function sendGAEvent(event) {
  window.ga('send', 'event', event);
}

/**
 * Sets if GA can send metrics.
 * @param {string} id The GA tracker ID.
 * @param {boolean} enabled True if the metrics is enabled.
 * @return {!Promise}
 */
async function setMetricsEnabled(id, enabled) {
  window[`ga-disable-${id}`] = !enabled;
}

export /** !GAHelperInterface */ {initGA, sendGAEvent, setMetricsEnabled};
