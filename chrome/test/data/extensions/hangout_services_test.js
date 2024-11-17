// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// JavaScript for invoking methods on APIs used by Hangouts via the
// Hangout Services extension, and a JavaScript-based end-to-end test
// of the extension.

// ID of the Hangout Services component extension.
var EXTENSION_ID = "nkeimhogjdpnpccoofpliimaahmaaome";

// Sends a message to the Hangout Services extension, expecting
// success, and unwraps the value returned.
function sendMessage(message, callback) {
  function unwrapValue(result) {
    if (callback)
      callback(result.value);
  }
  window.top.chrome.runtime.sendMessage(EXTENSION_ID, message, unwrapValue);
}

// If connected, this port will receive events from the extension.
var callbackPort;

//
// Helpers to invoke functions on the extension.
//
// Will call |callback(cpuInfo)| on completion.
function cpuGetInfo(callback) {
  sendMessage({'method': 'cpu.getInfo'}, callback);
}

// Will call |callback()| on completion.
function loggingSetMetadata(metaData, callback) {
  sendMessage({'method': 'logging.setMetadata', 'metaData': metaData},
              callback);
}

// Will call |callback()| on completion.
function loggingStart(callback) {
  sendMessage({'method': 'logging.start'}, callback);
}

// Will call |callback()| when API method has been called (it will
// complete later).
function loggingUploadOnRenderClose() {
  sendMessage({'method': 'logging.uploadOnRenderClose'});
}

// Will call |callback()| on completion.
function loggingStop(callback) {
  sendMessage({'method': 'logging.stop'}, callback);
}

// Will call |callback()| on completion.
function loggingStore(logId, callback) {
  sendMessage({'method': 'logging.store', 'logId': logId}, callback);
}

// Will call |callback()| on completion.
function loggingUploadStored(logId, callback) {
  sendMessage({'method': 'logging.uploadStored', 'logId': logId}, callback);
}

// Will call |callback(uploadResult)| on completion.
function loggingUpload(callback) {
  sendMessage({'method': 'logging.upload'}, callback);
}

// Will call |callback(uploadResult)| on completion.
function loggingStopAndUpload(callback) {
  sendMessage({'method': 'logging.stopAndUpload'}, callback);
}

// Will call |callback()| on completion.
function loggingDiscard(callback) {
  sendMessage({'method': 'logging.discard'}, callback);
}

// Will call |callback(hardwarePlatformInfo)| on completion.
function getHardwarePlatformInfo(callback) {
  sendMessage({'method': 'getHardwarePlatformInfo'}, callback);
}

//
// Automated tests.
//

// Very micro test framework. Add all tests to |TESTS|. Each test must
// call the passed-in callback eventually with a string indicating
// results. Empty results indicate success.
var TESTS = [
  testCpuGetInfo,
  testLogging,
  testLoggingSetMetaDataAfterStart,
  testEnabledLoggingButDiscard,
  testStoreLog,

  // Uncomment to manually test timeout logic.
  //testTimeout,
];

var TEST_TIMEOUT_MS = 3000;

function runAllTests(callback) {
  var results = '';

  // Run one test at a time, running the next only on completion.
  // This makes it easier to deal with timing out tests that do not
  // complete successfully.
  //
  // It also seems necessary (instead of starting all the tests in
  // parallel) as the webrtcLoggingPrivate API does not seem to like
  // certain sequences of interleaved requests (it may DCHECK in
  // WebRtcLoggingHandlerHost::NotifyLoggingStarted() when firing the
  // start callback.
  //
  // TODO(grunell): Fix webrtcLoggingPrivate to be stateless.

  // Index of the test currently executing.
  var testIndex = 0;

  function startTest(test) {
    console.log('Starting ' + test.name);

    // Start the test function...
    test(function(currentResults) {
        nextTest(test.name, currentResults, false);
      });

    // ...and also start a timeout.
    function onTimeout() {
      nextTest(test.name, '', true);
    }
    setTimeout(onTimeout, TEST_TIMEOUT_MS);
  }

  function nextTest(testName, currentResults, timedOut) {
    // The check for testIndex is necessary for timeouts arriving
    // after testIndex is already past the end of the TESTS array.
    if (testIndex >= TESTS.length ||
        testName != TESTS[testIndex].name) {
      // Either a timeout of a function that already completed, or a
      // function completing after a timeout. Either way we ignore.
      console.log('Ignoring results for ' + testName +
                  ' (timedout: ' + timedOut + ')');
      return;
    }

    if (timedOut) {
      console.log('Timed out: ' + testName);
      results = results + 'Timed out: ' + testName + '\n';
    } else {
      console.log('Got results for ' + testName + ': ' + currentResults);
      if (currentResults != '') {
        results = results + 'Failure in ' + testName + ':\n';
        results = results + currentResults;
      }
    }

    ++testIndex;
    if (testIndex == TESTS.length) {
      callback(results);
    } else {
      startTest(TESTS[testIndex]);
    }
  }

  startTest(TESTS[testIndex]);
}

function testCpuGetInfo(callback) {
  cpuGetInfo(function(info) {
      if (info.numOfProcessors != 0 &&
          info.archName != '' &&
          info.modelName != '') {
        callback('');
      } else {
        callback('Missing information in CpuInfo');
      }
    });
}

// Tests setting metadata, turning on upload, starting and then
// stopping the log.
function testLogging(callback) {
  loggingSetMetadata([{'bingo': 'bongo', 'smurf': 'geburf'}], function() {
      loggingStart(function() {
          loggingUploadOnRenderClose();
          loggingStop(function() {
              callback('');
            });
        });
    });
}

// Tests starting the log, setting metadata, turning on upload, and then
// stopping the log.
function testLoggingSetMetaDataAfterStart(callback) {
  loggingStart(function() {
      loggingSetMetadata([{'bingo': 'bongo', 'smurf': 'geburf'}], function() {
          loggingUploadOnRenderClose();
          loggingStop(function() {
              callback('');
            });
        });
    });
}

// Starts and stops logging while auto-upload is enabled, but
// requests logs be discarded after stopping logging.
function testEnabledLoggingButDiscard(callback) {
  loggingUploadOnRenderClose();
  loggingStart(function() {
      loggingStop(function() {
          loggingDiscard(function() {
              callback('');
            });
        });
    });
}

function testStoreLog(callback) {
  var logId = "mylog_id";
  // Start by logging something.
  loggingSetMetadata([{'bingos': 'bongos', 'smurfs': 'geburfs'}], function() {
      loggingStart(function() {
          loggingStop(function() {
              loggingStore(logId, function() {
                  loggingUploadStored(logId, function(loggingResult) {
                    if (loggingResult != '') {
                      callback('');
                    } else {
                      callback('Got empty upload result.');
                    }
                  });
              });
            });
        });
    });
}

function testGetHardwarePlatformInfo(callback) {
  getHardwarePlatformInfo(function(hardwarePlatformInfo) {
    if (hardwarePlatformInfo.hasOwnProperty('manufacturer') &&
        hardwarePlatformInfo.hasOwnProperty('model')) {
      callback('');
    } else {
      callback('Missing information in hardwarePlatformInfo');
    }
  });
}

function testTimeout(callback) {
  // Never call the callback. Used for manually testing that the
  // timeout logic of the test framework is correct.
}
