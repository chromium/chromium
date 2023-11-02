// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var inputElement;
var waitingForBlur = false;
var seenBlurAfterFocus = false;

var seenEvent = {};
var waitingEvent = {};

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(data) {
  embedder.postMessage(JSON.stringify(data), '*');
};

var waitForBlurAfterFocus = function() {
  LOG('seenBlurAfterFocus: ' + seenBlurAfterFocus);
  if (seenBlurAfterFocus) {
    // Already seen it.
    sendMessage(['response-seenBlurAfterFocus']);
    return;
  }

  // Otherwise we will wait.
  waitingForBlur = true;
};

var waitForFocus = function() {
  inputElement = document.createElement('input');
  inputElement.addEventListener('focus', function(e) {
    LOG('input.focus');
    sendMessage(['response-seenFocus']);

    var blurHandler = function(e) {
      seenBlurAfterFocus = true;
      if (waitingForBlur) {
        inputElement.removeEventListener('blur', blurHandler);
        sendMessage(['response-seenBlurAfterFocus']);
      }
    };
    inputElement.addEventListener('blur', blurHandler);
  });
  document.body.appendChild(inputElement);

  inputElement.focus();
};

var createInput = function(count) {
  inputElement = document.createElement('input');
  inputElement.eventCount = count;
  inputElement.addEventListener('click', function(e) {
    sendMessage(['response-elementClicked']);
  });
  inputElement.addEventListener('input', function(e) {
    inputElement.eventCount -= 1;
    if (inputElement.eventCount <= 0 && inputElement.signalOnInput) {
      sendMessage(['response-inputValue', inputElement.value]);
    }
  });

  document.body.appendChild(inputElement);
  sendMessage(['response-createdInput']);
};

var getInputValue = function() {
  if (inputElement.eventCount <= 0) {
    sendMessage(['response-inputValue', inputElement.value]);
  } else {
    inputElement.signalOnInput = true;
  }
}

var monitorEvent = function(type) {
  var listener = function(e) {
    seenEvent[type] = true;
    if (waitingEvent[type]) {
      sendMessage(['response-waitEvent', type]);
    }
    window.removeEventListener(type, listener);
  };
  seenEvent[type] = false;
  waitingEvent[type] = false;
  window.addEventListener(type, listener);
}

var waitEvent = function(type) {
  if (seenEvent[type]) {
    sendMessage(['response-waitEvent', type]);
  } else {
    waitingEvent[type] = true;
  }
}

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    embedder = e.source;
    sendMessage(['connected']);
  } else if (data[0] == 'request-hasFocus') {
    var hasFocus = document.hasFocus();
    sendMessage(['response-hasFocus', hasFocus]);
  } else if (data[0] == 'request-waitForFocus') {
    waitForFocus();
  } else if (data[0] == 'request-waitForBlurAfterFocus') {
    waitForBlurAfterFocus();
  } else if (data[0] == 'request-createInput') {
    createInput(data[1]);
  } else if (data[0] == 'request-getInputValue') {
    getInputValue();
  } else if (data[0] == 'request-monitorEvent') {
    monitorEvent(data[1]);
  } else if (data[0] == 'request-waitEvent') {
    waitEvent(data[1]);
  }
});

window.addEventListener('focus', function(e) {
  sendMessage(['focused']);
});

window.addEventListener('blur', function(e) {
  sendMessage(['blurred']);
});
