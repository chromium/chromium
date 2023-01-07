// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var inputElement = undefined;
var waitingForFocus = false;
var waitingForBlur = false;
var waitingForFocusAgain = false;
var numberOfFocusEvents = 0;
var numberOfBlurEvents = 0;

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(msg) {
  if (!embedder) {
    LOG('embedder not found to send message');
    return;
  }
  embedder.postMessage(JSON.stringify([msg]), '*');
};

var focusHandler = function(e) {
  LOG('focusHandler');
  ++numberOfFocusEvents;
  respondIfWaitingForFocus();
};

var blurHandler = function(e) {
  LOG('blurHandler');
  ++numberOfBlurEvents;
  respondIfWaitingForBlur();
  inputElement.removeEventListener('blur', blurHandler);
 };

var initialize = function() {
  inputElement = document.createElement('input');
  inputElement.addEventListener('focus', focusHandler);
  inputElement.addEventListener('blur', blurHandler);
  document.body.appendChild(inputElement);
};

// We respond back to the embedder if we are waiting for a focus event.
var respondIfWaitingForFocus = function() {
  if ((waitingForFocus || waitingForFocusAgain) && numberOfFocusEvents >= 1) {
    numberOfFocusEvents = 0;
    if (waitingForFocus) {
      sendMessage('response-focus');
    } else if (waitingForFocusAgain) {
      inputElement.removeEventListener('focus', focusHandler);
      sendMessage('response-focusAgain');
    }
    waitingForFocus = false;
    waitingForFocusAgain = false;
    return;
  }
};

// We respond back to the embedder if we are waiting for a blur event.
var respondIfWaitingForBlur = function() {
  if (waitingForBlur && numberOfBlurEvents >= 1) {
    numberOfBlurEvents = 0;
    waitingForBlur = false;
    // End of step 2.
    sendMessage('response-blur');

    numberOfFocusEvents = 0;
    waitingForFocus = false;
    return;
  }
};

// Wait until we see a focus event, in the case we already have seen a focus,
// this method will respond to the embedder immediately.
var waitForFocus = function() {
  waitingForFocus = true;
  respondIfWaitingForFocus();
};

// Wait until we see a blur event, in the case we already have seen a blur,
// this method will respond to the embedder immediately.
var waitForBlur = function() {
  waitingForBlur = true;
  respondIfWaitingForBlur();
};

// Wait until we see a focus event after a focus+blur. In the case we
// already have seen a focus after focus+blur, this method will respond to the
// embedder immediately.
var waitForFocusAgain = function() {
  waitingForFocusAgain = true;
  respondIfWaitingForFocus();
};

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  LOG('message, data: ' + data);

  if (data[0] == 'connect') {
    embedder = e.source;
    sendMessage('connected');
  } else if (data[0] == 'request-waitForFocus') {
    waitForFocus();
  } else if (data[0] == 'request-waitForBlur') {
    waitForBlur();
  } else if (data[0] == 'request-waitForFocusAgain') {
    waitForFocusAgain();
  }
});

document.addEventListener('click', function(e) {
  inputElement.focus();
});

initialize();
