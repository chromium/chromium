// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for WebDriver status codes.
 * @enum {number}
 */
var StatusCode = {
  OK: 0,
  UNKNOWN_ERROR: 13,
  JAVASCRIPT_ERROR: 17,
};

/**
* Execute the given script and save its asynchronous result.
*
* If script1 finishes after script2 is executed, then script1's result will be
* discarded while script2's will be saved.
*
* @param {string} script The asynchronous script to be executed. The script
*     should be a proper function body. It will be wrapped in a function and
*     invoked with the given arguments and, as the final argument, a callback
*     function to invoke to report the asynchronous result.
* @param {!Array<*>} args Arguments to be passed to the script.
* @param {boolean} isUserSupplied Whether the script is supplied by the user.
*     If not, UnknownError will be used instead of JavaScriptError if an
*     exception occurs during the script, and an additional error callback will
*     be supplied to the script.
*/
async function executeAsyncScript(script, args, isUserSupplied) {
  const Promise = window.cdc_adoQpoasnfa76pfcZLmcfl_Promise || window.Promise;
  const promiseResolve = Promise.resolve.bind(Promise);
  const promiseThen = Function.prototype.call.bind(Promise.prototype.then);
  function isThenable(value) {
    return value != null && typeof value === 'object' &&
        typeof value.then === 'function';
  }
  function reportValue(value) {
    return {status: StatusCode.OK, value: value};
  }
  function reportError(error) {
    var code = isUserSupplied ? StatusCode.JAVASCRIPT_ERROR :
                                (error.code || StatusCode.UNKNOWN_ERROR);
    var message = error.message;
    if (error.stack) {
      message += "\nJavaScript stack:\n" + error.stack;
    }
    return {status: code, value: message};
  }
  let resolveScript;
  let rejectScript;
  const scriptPromise = new Promise((resolve, reject) => {
    resolveScript = resolve;
    rejectScript = reject;
  });
  const resultPromise = promiseThen(scriptPromise, reportValue, reportError);

  args.push(resolveScript);
  if (!isUserSupplied) {
    args.push(rejectScript);
  }
  try {
    const scriptResult = new Function(script).apply(null, args);
    if (isThenable(scriptResult)) {
      const resolvedPromise = promiseResolve(scriptResult);
      if (isUserSupplied) {
        promiseThen(resolvedPromise, undefined, rejectScript);
      } else {
        promiseThen(resolvedPromise, resolveScript, rejectScript);
      }
    }
  } catch (error) {
    rejectScript(error);
  }

  return await resultPromise;
}
