// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert} from './chrome_util.js';
import * as Comlink from './lib/comlink.js';
import * as state from './state.js';
import {
  Facing,  // eslint-disable-line no-unused-vars
  Mode,
  PerfEvent,        // eslint-disable-line no-unused-vars
  PerfInformation,  // eslint-disable-line no-unused-vars
  Resolution,       // eslint-disable-line no-unused-vars
} from './type.js';
// eslint-disable-next-line no-unused-vars
import {GAHelperInterface} from './untrusted_helper_interfaces.js';
import * as util from './util.js';
import {WaitableEvent} from './waitable_event.js';

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
 * @type {!WaitableEvent}
 */
const ready = new WaitableEvent();

/**
 * @type {!Promise<!GAHelperInterface>}
 */
const gaHelper = (async () => {
  return /** @type {!GAHelperInterface} */ (await util.createUntrustedJSModule(
      '/js/untrusted_ga_helper.js', browserProxy.getUntrustedOrigin()));
})();

/**
 * Send the event to GA backend.
 * @param {!ga.Fields} event The event to send.
 * @param {?Map<number, !Object>=} dimen Optional object contains dimension
 *     information.
 */
async function sendEvent(event, dimen = null) {
  const assignDimension = (e, d) => {
    d.forEach((value, key) => e[`dimension${key}`] = value);
  };

  assert(baseDimen !== null);
  assignDimension(event, baseDimen);
  if (dimen !== null) {
    assignDimension(event, dimen);
  }

  await ready.wait();

  // This value reflects the logging constent option in OS settings.
  const canSendMetrics = await browserProxy.isMetricsAndCrashReportingEnabled();
  if (canSendMetrics) {
    (await gaHelper).sendGAEvent(event);
  }
}

/**
 * Set if the metrics is enabled. Note that the metrics will only be sent if it
 * is enabled AND the logging consent option is enabled in OS settings.
 * @param {boolean} enabled True if the metrics is enabled.
 */
export async function setMetricsEnabled(enabled) {
  await ready.wait();
  await (await gaHelper).setMetricsEnabled(GA_ID, enabled);
}

/**
 * Initializes metrics with parameters.
 */
export async function initMetrics() {
  const board = await browserProxy.getBoard();
  const boardName = /^(x86-)?(\w*)/.exec(board)[0];
  const match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
  const osVer = match ? match[1] : '';
  baseDimen = new Map([
    [1, boardName],
    [2, osVer],
  ]);

  const GA_LOCAL_STORAGE_KEY = 'google-analytics.analytics.user-id';
  const gaLocalStorage =
      await browserProxy.localStorageGet({[GA_LOCAL_STORAGE_KEY]: null});
  const clientId = gaLocalStorage[GA_LOCAL_STORAGE_KEY];

  const setClientId = (id) => {
    browserProxy.localStorageSet({[GA_LOCAL_STORAGE_KEY]: id});
  };

  await (await gaHelper)
      .initGA(
          GA_ID, clientId, browserProxy.shouldAddFakeHistory(),
          Comlink.proxy(setClientId));
  ready.signal();
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
     * @type {!PerfInformation|undefined} Optional information for the event.
     */
    this.perfInfo;
  }
}

/**
 * Sends perf type event.
 * @param {!PerfEventParam} param
 */
export function sendPerfEvent({event, duration, perfInfo = {}}) {
  const resolution = perfInfo['resolution'] || '';
  const facing = perfInfo['facing'] || '';
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

/**
 * Sends the barcode enabled event.
 */
export function sendBarcodeEnabledEvent() {
  sendEvent({
    eventCategory: 'barcode',
    eventAction: 'enable',
  });
}

/**
 * Types of the decoded barcode content.
 * @enum {string}
 */
export const BarcodeContentType = {
  TEXT: 'text',
  URL: 'url',
};

/**
 * @typedef {{
 *   contentType: !BarcodeContentType,
 * }}
 */
export let BarcodeDetectedEventParam;

/**
 * Sends the barcode detected event.
 * @param {!BarcodeDetectedEventParam} param
 */
export function sendBarcodeDetectedEvent({contentType}) {
  sendEvent({
    eventCategory: 'barcode',
    eventAction: 'detect',
    eventLabel: contentType,
  });
}
