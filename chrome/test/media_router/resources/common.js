/**
 * Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Common APIs for presentation integration tests.
 *
 */

var startSessionPromise = null;
var startedConnection = null;
var reconnectedSession = null;
var presentationUrl = null;
let params = (new URL(window.location.href)).searchParams;

if (params.get('__is_android__') == 'true') {
  // Android only accepts Cast Presentation URLs for the time being.
  presentationUrl = "cast:CCCCCCCC";
} else if (params.get('__oneUA__') == 'true') {
  presentationUrl = "presentation_receiver.html";
} else if (params.get('__oneUANoReceiver__') == 'true') {
  presentationUrl = "no_presentation_receiver.html";
} else {
  presentationUrl = "https://www.example.com/presentation.html";
}

var startSessionRequest = new PresentationRequest([presentationUrl]);
var defaultRequestSessionId = null;
var lastExecutionResult = null;
var useDomAutomationController = !!window.domAutomationController;

window.navigator.presentation.defaultRequest = startSessionRequest;
window.navigator.presentation.defaultRequest.onconnectionavailable = function(e)
{
  defaultRequestSessionId = e.connection.id;
};

/**
 * Waits until one sink is available.
 */
function waitUntilDeviceAvailable() {
  return startSessionRequest.getAvailability(presentationUrl).then(
  function(availability) {
    console.log('availability ' + availability.value + '\n');
    if (availability.value) {
      return stringifyAndSaveResult(true, '');
    } else {
      return new Promise(resolve => {
        availability.onchange = function(newAvailability) {
          if (newAvailability)
            resolve();
        }
      }).then(() => stringifyAndSaveResult(true, ''));
    }
  }).catch(function(e) {
    return stringifyAndSaveResult(false, 'got error: ' + e);
  });
}

/**
 * Starts session.
 */
function startSession() {
  startSessionPromise = startSessionRequest.start();
  console.log('start session');
  return stringifyAndSaveResult(true, '');
}

/**
 * Checks if the session has been started successfully.
 */
function checkSession() {
  if (!startSessionPromise) {
    return stringifyAndSaveResult(false, 'Did not attempt to start session');
  } else {
    return startSessionPromise.then(function(session) {
      if(!session) {
        return stringifyAndSaveResult(false,
          'Failed to start session: connection is null');
      } else {
        // set the new session
        startedConnection = session;
        return waitForConnectedStateAndSendResult(startedConnection);
      }
    }).catch(function(e) {
      // terminate old session if exists
      startedConnection && startedConnection.terminate();
      return stringifyAndSaveResult(false,
        'Failed to start session: encountered exception ' + e);
    })
  }
}

/**
 * Asserts the current state of the connection is 'connected' or 'connecting'.
 * If the current state is connecting, waits for it to become 'connected'.
 * @param {!PresentationConnection} connection
 */
function waitForConnectedStateAndSendResult(connection) {
  console.log(`connection state is "${connection.state}"`);
  if (connection.state == 'connected') {
    return stringifyAndSaveResult(true, '');
  } else if (connection.state == 'connecting') {
    return new Promise(resolve => {
      connection.onconnect = () => {
        resolve();
      };
    }).then(() => stringifyAndSaveResult(true, ''));
  } else {
    return stringifyAndSaveResult(false,
      `Expect connection state to be "connecting" or "connected", actual: \
      "${connection.state}"`);
  }
}

/**
 * Checks the start() request fails with expected error and message substring.
 * @param {!string} expectedErrorName
 * @param {!string} expectedErrorMessageSubstring
 */
function checkStartFailed(expectedErrorName, expectedErrorMessageSubstring) {
  if (!startSessionPromise) {
    return stringifyAndSaveResult(false, 'Did not attempt to start session');
  } else {
    return startSessionPromise.then(function(session) {
      return stringifyAndSaveResult(false, 'start() unexpectedly succeeded.');
    }).catch(function(e) {
      if (expectedErrorName != e.name) {
        return stringifyAndSaveResult(false,
          'Got unexpected error. ' + e.name + ': ' + e.message);
      } else if (e.message.indexOf(expectedErrorMessageSubstring) == -1) {
        return stringifyAndSaveResult(false,
            'Error message is not correct, it should contain "' +
            expectedErrorMessageSubstring + '"');
      } else {
        return stringifyAndSaveResult(true, '');
      }
    })
  }
}

/**
 * Terminates current session.
 */
function terminateSessionAndWaitForStateChange() {
  if (startedConnection) {
    return new Promise(resolve => {
      startedConnection.onterminate = function() {
        resolve();
      };
      startedConnection.terminate();
    }).then(() => stringifyAndSaveResult(true, ''));
  } else {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
}

/**
 * Closes |startedConnection| and waits for its onclose event.
 */
function closeConnectionAndWaitForStateChange() {
  if (startedConnection) {
    if (startedConnection.state == 'closed') {
      return stringifyAndSaveResult(false,
        'startedConnection is unexpectedly closed.');
    }
    return new Promise(resolve => {
      startedConnection.onclose = function() {
        resolve();
      };
      startedConnection.close();
    }).then(() => stringifyAndSaveResult(true, ''));
  } else {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
}

/**
 * Sends a message to |startedConnection| and expects InvalidStateError to be
 * thrown. Requires |startedConnection.state| to not equal |initialState|.
 */
function checkSendMessageFailed(initialState) {
  if (!startedConnection) {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
  if (startedConnection.state != initialState) {
    return stringifyAndSaveResult(false,
      'startedConnection.state is "' + startedConnection.state +
               '", but we expected "' + initialState + '".');
  }

  try {
    startedConnection.send('test message');
  } catch (e) {
    if (e.name == 'InvalidStateError') {
      return stringifyAndSaveResult(true, '');
    } else {
      return stringifyAndSaveResult(false,
        'Got an unexpected error: ' + e.name);
    }
  }
  return stringifyAndSaveResult(false,
    'Expected InvalidStateError but it was never thrown.');
}

/**
 * Sends a message, and expects the connection to close on error.
 */
function sendMessageAndExpectConnectionCloseOnError() {
  if (!startedConnection) {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
  return new Promise(resolve => {
    startedConnection.onclose = function(event) {
      var reason = event.reason;
      if (reason != 'error') {
        return resolve(stringifyAndSaveResult(false,
          'Unexpected close reason: ' + reason));
      }
      return resolve(stringifyAndSaveResult(true, ''));
    };
    startedConnection.send('foo');
  });
}

/**
 * Sends the given message, and expects response from the receiver.
 * @param {!string} message
 */
function sendMessageAndExpectResponse(message) {
  if (!startedConnection) {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
  if (startedConnection.state != 'connected') {
    return stringifyAndSaveResult(false,
        `Expected the connection state to be connected but it was \
         ${startedConnection.state}`);
  }
  return new Promise(resolve => {
    startedConnection.onmessage = function(receivedMessage) {
      var expectedResponse = 'Pong: ' + message;
      var actualResponse = receivedMessage.data;
      if (actualResponse != expectedResponse) {
        return resolve(stringifyAndSaveResult(false,
          'Expected message: ' + expectedResponse +
            ', but got: ' + actualResponse));
      }
      return resolve(stringifyAndSaveResult(true, ''));
    };
    startedConnection.send(message);
  });
}

/**
 * Sends 'close' to receiver page, and expects receiver page closing
 * the connection.
 */
function initiateCloseFromReceiverPage() {
  if (!startedConnection) {
    return stringifyAndSaveResult(false, 'startedConnection does not exist.');
  }
  if (startedConnection.state != 'connected') {
    return stringifyAndSaveResult(false,
        `Expected the connection state to be connected but it was \
         ${startedConnection.state}`);
  }
  return new Promise(resolve => {
    startedConnection.onclose = (event) => {
      const reason = event.reason;
      if (reason != 'closed') {
        return resolve(stringifyAndSaveResult(false,
          'Unexpected close reason: ' + reason));
      }
      return resolve(stringifyAndSaveResult(true, ''));
    };
    startedConnection.send('close');
  });
}

/**
 * Reconnects to |sessionId| and verifies that it succeeds.
 * @param {!string} sessionId ID of session to reconnect.
 */
function reconnectSession(sessionId) {
  var reconnectSessionRequest = new PresentationRequest(presentationUrl);
  return reconnectSessionRequest.reconnect(sessionId).then(function(session) {
    if (!session) {
      return stringifyAndSaveResult(false,
        'reconnectSession returned null session');
    } else {
      reconnectedSession = session;
      return waitForConnectedStateAndSendResult(reconnectedSession);
    }
  }).catch(function(error) {
    return stringifyAndSaveResult(false,
      'reconnectSession failed: ' + error.message);
  });
}

/**
 * Calls reconnect(sessionId) and verifies that it fails.
 * @param {!string} sessionId ID of session to reconnect.
 * @param {!string} expectedErrorMessage
 */
function reconnectSessionAndExpectFailure(sessionId, expectedErrorMessage) {
  var reconnectSessionRequest = new PresentationRequest(presentationUrl);
  return reconnectSessionRequest.reconnect(sessionId).then(function(session) {
    return stringifyAndSaveResult(false, 'reconnect() unexpectedly succeeded.');
  }).catch(function(error) {
    if (error.message.indexOf(expectedErrorMessage) > -1) {
      return stringifyAndSaveResult(true, '');
    } else {
      return stringifyAndSaveResult(false,
        'Error message mismatch. Expected: ' + expectedErrorMessage +
        ', actual: ' + error.message);
    }
  });
}

/**
 * Sends the test result back to browser test.
 * @param passed true if test passes, otherwise false.
 * @param errorMessage empty string if test passes, error message if test
 *                      fails.
 */
function stringifyAndSaveResult(passed, errorMessage) {
  return lastExecutionResult = JSON.stringify({
    passed,
    errorMessage,
  });
}
