// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert} from './chrome_util.js';
// eslint-disable-next-line no-unused-vars
import {PerfEvent} from './perf.js';
import * as state from './state.js';
import {
  Facing,  // eslint-disable-line no-unused-vars
  Mode,
  Resolution,  // eslint-disable-line no-unused-vars
} from './type.js';

/**
 * The tracker ID of the GA metrics.
 * @type {string}
 */
const GA_ID = 'UA-134822711-1';

/**
 * @type {?Map<number, !Object>}
 */
let baseDimen = null;

/**
 * @type {?Promise}
 */
let ready = null;

/**
 * @type {boolean}
 */
let isMetricsEnabled = false;

/**
 * Disable metrics sending if either the logging consent option is disabled or
 * metrics is disabled for current session. (e.g. Running tests)
 * @return {!Promise}
 */
async function disableMetricsIfNotAllowed() {
  // This value reflects the logging constent option in OS settings.
  const canSendMetrics = await browserProxy.isMetricsAndCrashReportingEnabled();
  window[`ga-disable-${GA_ID}`] = !isMetricsEnabled || !canSendMetrics;
}

/**
 * Send the event to GA backend.
 * @param {!ga.Fields} event The event to send.
 * @param {?Map<number, !Object>=} dimen Optional object contains dimension
 *     information.
 */
async function sendEvent(event, dimen = null) {
  assert(window.ga !== null);
  assert(ready !== null);
  await ready;
  await disableMetricsIfNotAllowed();

  const assignDimension = (e, d) => {
    d.forEach((value, key) => e[`dimension${key}`] = value);
  };

  assert(baseDimen !== null);
  assignDimension(event, baseDimen);
  if (dimen !== null) {
    assignDimension(event, dimen);
  }
  window.ga('send', 'event', event);
}

/**
 * Set if the metrics is enabled. Note that the metrics will only be sent if it
 * is enabled AND the logging consent option is enabled in OS settings.
 * @param {boolean} enabled True if the metrics is enabled.
 */
export function setMetricsEnabled(enabled) {
  assert(ready !== null);
  isMetricsEnabled = enabled;
}

/**
 * Initializes metrics with parameters.
 */
export function initMetrics() {
  ready = (async () => {
    browserProxy.addDummyHistoryIfNotAvailable();

    // GA initialization function which is mostly copied from
    // https://developers.google.com/analytics/devguides/collection/analyticsjs.
    (function(i, s, o, g, r) {
      i['GoogleAnalyticsObject'] = r;
      i[r] = i[r] || function(...args) {
        (i[r].q = i[r].q || []).push(args);
      }, i[r].l = new Date().getTime();
      const a = s.createElement(o);
      const m = s.getElementsByTagName(o)[0];
      a['async'] = 1;
      a['src'] = g;
      m.parentNode.insertBefore(a, m);
    })(window, document, 'script', '../js/lib/analytics.js', 'ga');

    const board = await browserProxy.getBoard();
    const boardName = /^(x86-)?(\w*)/.exec(board)[0];
    const match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
    const osVer = match ? match[1] : '';
    baseDimen = new Map([
      [1, boardName],
      [2, osVer],
    ]);

    // By default GA stores the user ID in cookies. Change to store in local
    // storage instead.
    const GA_LOCAL_STORAGE_KEY = 'google-analytics.analytics.user-id';
    const gaLocalStorage =
        await browserProxy.localStorageGet({[GA_LOCAL_STORAGE_KEY]: null});
    window.ga('create', GA_ID, {
      'storage': 'none',
      'clientId': gaLocalStorage[GA_LOCAL_STORAGE_KEY] || null,
    });
    window.ga(
        (tracker) => browserProxy.localStorageSet(
            {[GA_LOCAL_STORAGE_KEY]: tracker.get('clientId')}));

    // By default GA uses a dummy image and sets its source to the target URL to
    // record metrics. Since requesting remote image violates the policy of
    // a platform app, use navigator.sendBeacon() instead.
    window.ga('set', 'transport', 'beacon');

    // By default GA only accepts "http://" and "https://" protocol. Bypass the
    // check here since we are "chrome-extension://".
    window.ga('set', 'checkProtocolTask', null);
  })();

  ready.then(async () => {
    // The metrics is default enabled.
    await setMetricsEnabled(true);
  });
}

/**
 * Parameters for logging launch event. |ackMigrate| stands for whether
 * the user acknowledged to migrate during launch.
 * @typedef {{ackMigrate: boolean}}
 */
export let LaunchEventParam;

/**
 * Sends launch type event.
 * @param {!LaunchEventParam} param
 */
export function sendLaunchEvent({ackMigrate}) {
  sendEvent({
    eventCategory: 'launch',
    eventAction: 'start',
    eventLabel: ackMigrate ? 'ack-migrate' : '',
  });
}

/**
 * Types of intent result dimension.
 * @enum {string}
 */
export const IntentResultType = {
  NOT_INTENT: '',
  CANCELED: 'canceled',
  CONFIRMED: 'confirmed',
};

/**
 * Types of different ways to trigger shutter button.
 * @enum {string}
 */
export const ShutterType = {
  UNKNOWN: 'unknown',
  MOUSE: 'mouse',
  KEYBOARD: 'keyboard',
  TOUCH: 'touch',
  VOLUME_KEY: 'volume-key',
};

/**
 * Parameters of capture metrics event.
 * @record
 */
export class CaptureEventParam {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!Facing} Camera facing of the capture.
     */
    this.facing;

    /**
     * @type {(number|undefined)} Length of 1 minute buckets for captured video.
     */
    this.duration;

    /**
     * @type {!Resolution} Capture resolution.
     */
    this.resolution;

    /**
     * @type {!IntentResultType|undefined}
     */
    this.intentResult;

    /**
     * @type {!ShutterType}
     */
    this.shutterType;

    /**
     * Whether the event is for video snapshot.
     * @type {boolean|undefined}
     */
    this.isVideoSnapshot;

    /**
     * Whether the video have ever paused and resumed in the recording.
     * @type {boolean|undefined}
     */
    this.everPaused;
  }
}

/**
 * Sends capture type event.
 * @param {!CaptureEventParam} param
 */
export function sendCaptureEvent({
  facing,
  duration = 0,
  resolution,
  intentResult = IntentResultType.NOT_INTENT,
  shutterType,
  isVideoSnapshot = false,
  everPaused = false,
}) {
  /**
   * @param {!Array<!state.StateUnion>} states
   * @param {!state.StateUnion=} cond
   * @param {boolean=} strict
   * @return {string}
   */
  const condState = (states, cond = undefined, strict = undefined) => {
    // Return the first existing state among the given states only if there is
    // no gate condition or the condition is met.
    const prerequisite = !cond || state.get(cond);
    if (strict && !prerequisite) {
      return '';
    }
    return prerequisite && states.find((s) => state.get(s)) || 'n/a';
  };

  const State = state.State;
  sendEvent(
      {
        eventCategory: 'capture',
        eventAction: condState(Object.values(Mode)),
        eventLabel: facing,
        eventValue: duration,
      },
      new Map([
        // Skips 3rd dimension for obsolete 'sound' state.
        [4, condState([State.MIRROR])],
        [
          5,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID),
        ],
        [6, condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER)],
        [7, condState([State.MIC], Mode.VIDEO, true)],
        [8, condState([State.MAX_WND])],
        [9, condState([State.TALL])],
        [10, resolution.toString()],
        [11, condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true)],
        [12, intentResult],
        [21, shutterType],
        [22, isVideoSnapshot],
        [23, everPaused],
      ]));
}


/**
 * Parameters for logging perf event.
 * @record
 */
export class PerfEventParam {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!PerfEvent} Target event type.
     */
    this.event;

    /**
     * @type {number} Duration of the event in ms.
     */
    this.duration;

    /**
     * @type {!Object|undefined} Optional information for the event.
     */
    this.extras;
  }
}

/**
 * Sends perf type event.
 * @param {!PerfEventParam} param
 */
export function sendPerfEvent({event, duration, extras = {}}) {
  const resolution = extras['resolution'] || '';
  const facing = extras['facing'] || '';
  sendEvent(
      {
        eventCategory: 'perf',
        eventAction: event,
        eventLabel: facing,
        // Round the duration here since GA expects that the value is an
        // integer. Reference:
        // https://support.google.com/analytics/answer/1033068
        eventValue: Math.round(duration),
      },
      new Map([
        [10, `${resolution}`],
      ]));
}

/**
 * See Intent class in intent.js for the descriptions of each field.
 * TODO(b/131133953): Pass an Intent directly once the type-only import feature
 * is implemented in Closure Compiler.
 * @typedef {{
 *   mode: !Mode,
 *   result: !IntentResultType,
 *   shouldHandleResult: boolean,
 *   shouldDownScale: boolean,
 *   isSecure: boolean,
 * }}
 */
export let IntentEventParam;

/**
 * Sends intent type event.
 * @param {!IntentEventParam} param
 */
export function sendIntentEvent(
    {mode, result, shouldHandleResult, shouldDownScale, isSecure}) {
  const getBoolValue = (b) => b ? '1' : '0';
  sendEvent(
      {
        eventCategory: 'intent',
        eventAction: mode,
        eventLabel: result,
      },
      new Map([
        [12, result],
        [13, getBoolValue(shouldHandleResult)],
        [14, getBoolValue(shouldDownScale)],
        [15, getBoolValue(isSecure)],
      ]));
}

/**
 * @typedef {{
 *   type: string,
 *   level: string,
 *   errorName: string,
 *   fileName: string,
 *   funcName: string,
 *   lineNo: string,
 *   colNo: string,
 * }}
 */
export let ErrorEventParam;

/**
 * Sends error type event.
 * @param {!ErrorEventParam} param
 */
export function sendErrorEvent(
    {type, level, errorName, fileName, funcName, lineNo, colNo}) {
  sendEvent(
      {
        eventCategory: 'error',
        eventAction: type,
        eventLabel: level,
      },
      new Map([
        [16, errorName],
        [17, fileName],
        [18, funcName],
        [19, lineNo],
        [20, colNo],
      ]));
}
