// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {AppWindow} from './app_window.js';
import {assertInstanceof} from './chrome_util.js';
import * as metrics from './metrics.js';
import {
  ErrorInfo,  // eslint-disable-line no-unused-vars
  ErrorLevel,
  ErrorType,
} from './type.js';

/**
 * Code location of stack frame.
 * @typedef {{
 *   fileName: string,
 *   funcName: string,
 *   lineNo: number,
 *   colNo : number,
 *  }}
 */
export let StackFrame;

const PRODUCT_NAME = 'ChromeOS_CameraApp';

/**
 * Converts v8 CallSite object to StackFrame.
 * @param {!CallSite} callsite
 * @return {!StackFrame}
 */
function toStackFrame(callsite) {
  // TODO(crbug.com/1072700): Handle native frame.
  let fileName = callsite.getFileName() || 'unknown';
  if (fileName.startsWith(window.location.origin)) {
    fileName = fileName.substring(window.location.origin.length + 1);
  }
  const ensureNumber = (n) => (n === undefined ? -1 : n);
  return {
    fileName,
    funcName: callsite.getFunctionName() || '[Anonymous]',
    lineNo: ensureNumber(callsite.getLineNumber()),
    colNo: ensureNumber(callsite.getColumnNumber()),
  };
}

/**
 * Gets stack frames from error.
 * @param {!Error} error
 * @return {?Array<!StackFrame>} return null if failed to get frames from error.
 */
export function getStackFrames(error) {
  const prevPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = (error, stack) => {
    try {
      return stack.map(toStackFrame);
    } catch (e) {
      console.warn('Failed to prepareStackTrace', e);
      return null;
    }
  };

  const /** (?Array<!StackFrame>|string) */ frames = error.stack;
  Error.prepareStackTrace = prevPrepareStackTrace;

  if (typeof frames !== 'object') {
    return null;
  }
  return /** @type {?Array<!StackFrame>} */ (frames);
}

/**
 * Gets the description text for an error.
 * @param {!Error} error
 * @return {string}
 */
function getErrorDescription(error) {
  return `${error.name}: ${error.message}`;
}

/**
 * Gets formatted string stack from error.
 * @param {!Error} error
 * @param {?Array<!StackFrame>} frames
 * @return {string}
 */
function formatErrorStack(error, frames) {
  const errorDesc = getErrorDescription(error);
  return errorDesc +
      (frames || [])
          .map(({fileName, funcName, lineNo, colNo}) => {
            let position = '';
            if (lineNo !== -1) {
              position = `:${lineNo}`;
              if (colNo !== -1) {
                position += `:${colNo}`;
              }
            }
            return `\n    at ${funcName} (${fileName}${position})`;
          })
          .join('');
}

/**
 * @type {?AppWindow}
 */
const appWindow = window['appWindow'];

/**
 * Initializes error collecting functions.
 */
export function initialize() {
  window.addEventListener('unhandledrejection', (e) => {
    reportError(
        ErrorType.UNCAUGHT_PROMISE, ErrorLevel.ERROR,
        assertInstanceof(e.reason, Error));
  });
}

/**
 * All triggered error will be hashed and saved in this set to prevent the same
 * error being triggered multiple times.
 * @type {!Set<string>}
 */
const triggeredErrorSet = new Set();

/**
 * Reports error either through test error callback in test run or to error
 * metrics in non test run.
 * @param {!ErrorType} type
 * @param {!ErrorLevel} level
 * @param {!Error} error
 */
export function reportError(type, level, error) {
  // Uncaught promise is already logged in console.
  if (type !== ErrorType.UNCAUGHT_PROMISE) {
    if (level === ErrorLevel.ERROR) {
      console.error(type, error);
    } else if (level === ErrorLevel.WARNING) {
      console.warn(type, error);
    }
  }

  const time = Date.now();
  const frames = getStackFrames(error);
  const errorName = error.name;
  const errorDesc = getErrorDescription(error);
  const frame = (frames !== null && frames.length > 0) ? frames[0] : {};
  const {fileName = '', lineNo = 0, colNo = 0, funcName = ''} = frame;

  const hash = [errorName, fileName, String(lineNo), String(colNo)].join(',');
  if (triggeredErrorSet.has(hash)) {
    return;
  }
  triggeredErrorSet.add(hash);

  const stackStr = formatErrorStack(error, frames);
  if (appWindow !== null) {
    appWindow.reportError({
      type,
      level,
      stack: stackStr,
      time,
      name: errorName,
    });
    return;
  }
  metrics.sendErrorEvent({
    type,
    level,
    errorName,
    fileName,
    funcName,
    lineNo: String(lineNo),
    colNo: String(colNo),
  });

  // Only reports the error to crash server if it reaches "error" level.
  if (level !== ErrorLevel.ERROR) {
    return;
  }

  /** @type {!chrome.crashReportPrivate.ErrorInfo} */
  const params = {
    product: PRODUCT_NAME,
    url: self.location.href,
    message: `${type}: ${errorDesc}`,
    lineNumber: lineNo || 0,
    stackTrace: stackStr || '',
    columnNumber: colNo || 0,
  };

  chrome.crashReportPrivate.reportError(params, () => {});
}
