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
  SCRIPT_TIMEOUT: 28,
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
* @param {boolean} timeout The duration in ms to keep the returned promise from
* being garbage collected.
*/
async function executeAsyncScript(script, args, isUserSupplied, timeout) {
  const Promise = window.cdc_adoQpoasnfa76pfcZLmcfl_Promise || window.Promise;
  function isThenable(value) {
    return typeof value === 'object' && typeof value.then === 'function';
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
  var promise = new Promise((resolve, reject) => {
    args.push(resolve);
    if (!isUserSupplied) {
      args.push(reject);
    }
    try {
      let scriptResult = new Function(script).apply(null, args);
      if (isThenable(scriptResult)) {
        const resolvedPromise = Promise.resolve(scriptResult);
        resolvedPromise.then((value) => {
          // Must be thenable if user-supplied.
          if (!isUserSupplied || isThenable(value))
            resolve(value);
        })
        .catch(reject);
      }
    } catch (error) {
      reject(error);
    }
  });

  if (typeof timeout !== 'undefined') {
    setTimeout(() => {return promise;}, timeout);
  }
  return await promise.then((result) => {
    return reportValue(result);
  }).catch((error) => {
    return reportError(error);
  });
}
