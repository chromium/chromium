// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = chrome || {};

/**
 * Organizes all signin event listeners and asynchronous requests.
 * This object has no public constructor.
 * @type {Object}
 */
chrome.signin = chrome.signin || {};

(function() {

// TODO(vishwath): This function is identical to the one in sync_internals.js
// Merge both if possible.
// Accepts a DOM node and sets its highlighted attribute oldVal != newVal
function highlightIfChanged(node, oldVal, newVal) {
  var oldStr = oldVal.toString();
  var newStr = newVal.toString();
  if (oldStr != '' && oldStr != newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd',
                          function() { this.removeAttribute('highlighted'); },
                          false);
    node.setAttribute('highlighted', '');
  }
}

// Wraps highlightIfChanged for multiple conditions.
function highlightIfAnyChanged(node, oldToNewValList) {
  for (var i = 0; i < oldToNewValList.length; i++)
    highlightIfChanged(node, oldToNewValList[i][0], oldToNewValList[i][1]);
}

function setClassFromValue(value) {
  if (value == 0)
    return 'zero';
  if (value == 'Successful')
    return 'ok';

  return '';
}

// Allow signin_index.html to access the functions above using the
// corresponding chrome.signin<method> calls.
chrome.signin['highlightIfChanged'] = highlightIfChanged;
chrome.signin['highlightIfAnyChanged'] = highlightIfAnyChanged;
chrome.signin['setClassFromValue'] = setClassFromValue;

// Simplified Event class, borrowed (ok, stolen) from chrome_sync.js
function Event() {
  this.listeners_ = [];
}

// Add a new listener to the list.
Event.prototype.addListener = function(listener) {
  this.listeners_.push(listener);
};

// Remove a listener from the list.
Event.prototype.removeListener = function(listener) {
  var i = this.findListener_(listener);
  if (i == -1) {
    return;
  }
  this.listeners_.splice(i, 1);
};

// Check if the listener has already been registered so we can prevent
// duplicate registrations.
Event.prototype.hasListener = function(listener) {
  return this.findListener_(listener) > -1;
};

// Are there any listeners registered yet?
Event.prototype.hasListeners = function() {
  return this.listeners_.length > 0;
};

// Returns the index of the given listener, or -1 if not found.
Event.prototype.findListener_ = function(listener) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] == listener) {
      return i;
    }
  }
  return -1;
};

// Fires the event.  Called by the actual event callback.  Any
// exceptions thrown by a listener are caught and logged.
Event.prototype.fire = function() {
  var args = Array.prototype.slice.call(arguments);
  for (var i = 0; i < this.listeners_.length; i++) {
    try {
      this.listeners_[i].apply(null, args);
    } catch (e) {
      if (e instanceof Error) {
        // Non-standard, but useful.
        console.error(e.stack);
      } else {
        console.error(e);
      }
    }
  }
};

// These are the events that will be registered.
chrome.signin.events = {
  'signin_manager': [
    'onSigninInfoChanged',
    'onCookieAccountsFetched'
 ]
};

for (var eventType in chrome.signin.events) {
  var events = chrome.signin.events[eventType];
  for (var i = 0; i < events.length; ++i) {
    var event = events[i];
    chrome.signin[event] = new Event();
  }
}

// Creates functions that call into SigninInternalsUI.
function makeSigninFunction(name) {
  var callbacks = [];

  // Calls the function, assuming the last argument is a callback to be
  // called with the return value.
  var fn = function() {
    var args = Array.prototype.slice.call(arguments);
    callbacks.push(args.pop());
    chrome.send(name, args);
  };

  // Handle a reply, assuming that messages are processed in FIFO order.
  // Called by SigninInternalsUI::HandleJsReply().
  fn.handleReply = function() {
    var args = Array.prototype.slice.call(arguments);
    // Remove the callback before we call it since the callback may
    // throw.
    var callback = callbacks.shift();
    callback.apply(null, args);
  };

  return fn;
}

// The list of js functions that call into SigninInternalsUI
var signinFunctions = [
  // Signin Summary Info
  'getSigninInfo'
];

for (var i = 0; i < signinFunctions.length; ++i) {
  var signinFunction = signinFunctions[i];
  chrome.signin[signinFunction] = makeSigninFunction(signinFunction);
}

chrome.signin.internalsInfo = {};

// Replace the displayed values with the latest fetched ones.
function refreshSigninInfo(signinInfo) {
  chrome.signin.internalsInfo = signinInfo;
  jstProcess(new JsEvalContext(signinInfo), $('signin-info'));
  jstProcess(new JsEvalContext(signinInfo), $('token-info'));
  jstProcess(new JsEvalContext(signinInfo), $('account-info'));
  jstProcess(new JsEvalContext(signinInfo), $('refresh-token-events'));
}

// Replace the cookie information with the fetched values.
function updateCookieAccounts(cookieAccountsInfo) {
  jstProcess(new JsEvalContext(cookieAccountsInfo), $('cookie-info'));
}

// On load, do an initial refresh and register refreshSigninInfo to be invoked
// whenever we get new signin information from SigninInternalsUI.
function onLoad() {
  chrome.signin.getSigninInfo(refreshSigninInfo);

  chrome.signin.onSigninInfoChanged.addListener(refreshSigninInfo);
  chrome.signin.onCookieAccountsFetched.addListener(updateCookieAccounts);
}

document.addEventListener('DOMContentLoaded', onLoad, false);
})();
