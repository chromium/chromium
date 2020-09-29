// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './chrome_util.js';
import * as metrics from './metrics.js';

/**
 * Types of error used in ERROR metrics.
 * @enum {string}
 */
export const ErrorType = {
  BROKEN_THUMBNAIL: 'broken-thumbnail',
  UNCAUGHT_PROMISE: 'uncaught-promise',
};

/**
 * Error level used in ERROR metrics.
 * @enum {string}
 */
export const ErrorLevel = {
  WARNING: 'WARNING',
  ERROR: 'ERROR',
};

/**
 * Error reported in testing run.
 * @typedef {{
 *   type: !ErrorType,
 *   level: !ErrorLevel,
 *   stack: string,
 *   time: number,
 * }}
 */
let ErrorInfo;  // eslint-disable-line no-unused-vars

/**
 * Callback for reporting error in testing run.
 * @typedef {function(!ErrorInfo)}
 */
export let TestingErrorCallback;

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

/**
 * Throws when a method is not implemented.
 */
export class NotImplementedError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'Method is not implemented') {
    super(message);
    this.name = this.constructor.name;
  }
}

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
      console.error('Failed to prepareStackTrace', e);
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
 * Gets formatted string stack from error.
 * @param {!Error} error
 * @return {string}
 */
export function formatErrorStack(error) {
  if (typeof error.stack === 'string') {
    return error.stack;
  }
  const errorString = error.name + ': ' + error.message;
  const frames = error.stack || /** @type {!Array<!StackFrame>} */ ([]);

  return errorString +
      frames
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
 * @type {?TestingErrorCallback}
 */
let onTestingError = null;

/**
 * Initializes error collecting functions.
 * @param {?TestingErrorCallback} onError Callback for reporting error in
 *     testing run. Set to null in non testing run.
 */
export function initialize(onError) {
  onTestingError = onError;
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
  // uncaught promise is already logged in console
  if (type !== ErrorType.UNCAUGHT_PROMISE) {
    if (level === ErrorLevel.ERROR) {
      console.error(type, error);
    } else {
      console.warn(type, error);
    }
  }
  const time = Date.now();
  const frames = getStackFrames(error);
  const errorName = error.name;
  const frame = (frames !== null && frames.length > 0) ? frames[0] : {};
  let {fileName = '', lineNo = '', colNo = '', funcName = ''} = frame;
  lineNo = String(lineNo);
  colNo = String(colNo);

  const hash = [errorName, fileName, lineNo, colNo].join(',');
  if (triggeredErrorSet.has(hash)) {
    return;
  }
  triggeredErrorSet.add(hash);

  if (onTestingError !== null) {
    onTestingError({type, level, stack: formatErrorStack(error), time});
    return;
  }
  metrics.sendErrorEvent(
      {type, level, errorName, fileName, funcName, lineNo, colNo});
}
