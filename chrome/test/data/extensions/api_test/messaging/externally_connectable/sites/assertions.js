// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

// We are going to kill all of the builtins, so hold onto the ones we need.
var defineProperty = Object.defineProperty;
var Error = window.Error;
var forEach = Array.prototype.forEach;
var push = Array.prototype.push;
var hasOwnProperty = Object.prototype.hasOwnProperty;
var getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
var getOwnPropertyNames = Object.getOwnPropertyNames;
var stringify = JSON.stringify;

// Kill all of the builtins functions to give us a fairly high confidence that
// the environment our bindings run in can't interfere with our code.
// These are taken from the ECMAScript spec.
var builtinTypes = [
  Object, Function, Array, String, Boolean, Number, Math, Date, RegExp, JSON,
];

function clobber(obj, name, qualifiedName) {
  // Clobbering constructors would break everything.
  // Clobbering toString is annoying.
  // Clobbering __proto__ breaks in ways that grep can't find.
  // Clobbering function name will break because
  // SafeBuiltins does not support getters yet. See crbug.com/463526.
  // Clobbering Function.call would make it impossible to implement these tests.
  // Clobbering Object.valueOf breaks v8.
  // Clobbering %FunctionPrototype%.caller and .arguments will break because
  // these properties are poisoned accessors in ES6.
  if (name == 'constructor' ||
      name == 'toString' ||
      name == '__proto__' ||
      name == 'name' && typeof obj == 'function' ||
      qualifiedName == 'Function.call' ||
      (obj !== Function && qualifiedName == 'Function.caller') ||
      (obj !== Function && qualifiedName == 'Function.arguments') ||
      qualifiedName == 'Object.valueOf') {
    return;
  }
  var desc = getOwnPropertyDescriptor(obj, name);
  if (!desc.configurable) return;
  var new_desc;
  if (desc.get || desc.set || typeof desc.value !== 'function') {
    new_desc =
        { get: function() {
                 throw new Error('Clobbered ' + qualifiedName + ' getter');
               },
          set: function(x) {
                 throw new Error('Clobbered ' + qualifiedName + ' setter');
               },
        };
  } else {
    new_desc =
        { value: function() {
                   throw new Error('Clobbered ' + qualifiedName + ' function');
                 }
        };
  }
  defineProperty(obj, name, new_desc);
}

forEach.call(builtinTypes, function(builtin) {
  var prototype = builtin.prototype;
  var typename = '<unknown>';
  if (prototype) {
    typename = prototype.constructor.name;
    forEach.call(getOwnPropertyNames(prototype), function(name) {
      clobber(prototype, name, typename + '.' + name);
    });
  }
  forEach.call(getOwnPropertyNames(builtin), function(name) {
    clobber(builtin, name, typename + '.' + name);
  });
  if (builtin.name)
    clobber(window, builtin.name, 'window.' + builtin.name);
});

// Codes for test results. Must match ExternallyConnectableMessagingTest::Result
// in c/b/extensions/extension_messages_apitest.cc.
var results = {
  OK: 0,
  NAMESPACE_NOT_DEFINED: 1,
  FUNCTION_NOT_DEFINED: 2,
  COULD_NOT_ESTABLISH_CONNECTION_ERROR: 3,
  OTHER_ERROR: 4,
  INCORRECT_RESPONSE_SENDER: 5,
  INCORRECT_RESPONSE_MESSAGE: 6,
};

class ResultError extends Error {
  constructor(result) {
    super('ResultError');
    this.result = result;
  }
}

// Make the messages sent vaguely complex, but unambiguously JSON-ifiable.
var kMessage = [{'a': {'b': 10}}, 20, 'c\x10\x11'];

// Text of the error message set in |chrome.runtime.lastError| when the
// messaging target does not exist.
var kCouldNotEstablishConnection =
    'Could not establish connection. Receiving end does not exist.';

// Our tab's location. Normally this would be our document's location but if
// we're an iframe it will be the location of the parent - in which case,
// expect to be told.
var tabLocationHref = null;

if (parent == window) {
  tabLocationHref = document.location.href;
} else {
  window.addEventListener('message', function listener(event) {
    window.removeEventListener('message', listener);
    tabLocationHref = event.data;
  });
}

function throwResultError(errorMessage) {
  if (errorMessage == kCouldNotEstablishConnection) {
    throw new ResultError(results.COULD_NOT_ESTABLISH_CONNECTION_ERROR);
  }
  throw new ResultError(results.OTHER_ERROR);
}

function checkResponse(response, expectedMessage, isApp) {
  // The response will be an echo of both the original message *and* the
  // MessageSender (with the tab field stripped down).
  //
  // First check the sender was correct.
  var incorrectSender = false;
  if (!isApp) {
    // Only extensions get access to a 'tab' property.
    if (!hasOwnProperty.call(response.sender, 'tab')) {
      console.warn('Expected a tab, got none');
      incorrectSender = true;
    }
    if (response.sender.tab.url != tabLocationHref) {
      console.warn('Expected tab url ' + tabLocationHref + ' got ' +
                   response.sender.tab.url);
      incorrectSender = true;
    }
  }
  if (hasOwnProperty.call(response.sender, 'id')) {
    console.warn('Expected no id, got "' + response.sender.id + '"');
    incorrectSender = true;
  }
  if (response.sender.url != document.location.href) {
    console.warn('Expected url ' + document.location.href + ' got ' +
                 response.sender.url);
    incorrectSender = true;
  }
  if (incorrectSender) {
    throw new ResultError(results.INCORRECT_RESPONSE_SENDER);
  }

  // Check the correct content was echoed.
  var expectedJson = stringify(expectedMessage);
  var actualJson = stringify(response.message);
  if (actualJson == expectedJson)
    return;
  console.warn('Expected message ' + expectedJson + ' got ' + actualJson);
  throw new ResultError(results.INCORRECT_RESPONSE_MESSAGE);
}

function sendToBrowserForTlsChannelId(result) {
  // Because the TLS channel ID tests read the TLS either an error code or the
  // TLS channel ID string from the same value, they require the result code
  // to be sent as a string.
  // String() is clobbered, so coerce string creation with +.
  return "" + result;
}

function checkRuntime() {
  if (!chrome.runtime) {
    throw new ResultError(results.NAMESPACE_NOT_DEFINED);
  }

  if (!chrome.runtime.connect || !chrome.runtime.sendMessage) {
    throw new ResultError(results.FUNCTION_NOT_DEFINED);
  }
}

function checkTlsChannelIdResponse(response) {
  if (chrome.runtime.lastError) {
    if (chrome.runtime.lastError.message == kCouldNotEstablishConnection)
      return sendToBrowserForTlsChannelId(
          results.COULD_NOT_ESTABLISH_CONNECTION_ERROR);
    return sendToBrowserForTlsChannelId(results.OTHER_ERROR);
  }
  if (response.sender.tlsChannelId !== undefined)
    return sendToBrowserForTlsChannelId(response.sender.tlsChannelId);
  return sendToBrowserForTlsChannelId('');
}

window.actions = {
  appendIframe: function(src) {
    var iframe = document.createElement('iframe');
    // When iframe has loaded, notify it of our tab location (probably
    // document.location) to use in its assertions, then continue.
    return new Promise(resolve => {
      iframe.addEventListener('load', function listener() {
        iframe.removeEventListener('load', listener);
        iframe.contentWindow.postMessage(tabLocationHref, '*');
        resolve(true);
      });
      iframe.src = src;
      document.body.appendChild(iframe);
    });
  }
};

window.assertions = {
  canConnectAndSendMessages: async function(extensionId, isApp, message) {
    try {
      checkRuntime();

      if (!message) {
        message = kMessage;
      }

      async function canSendMessage() {
        const response = await new Promise((resolve, reject) => {
          chrome.runtime.sendMessage(
              extensionId, message, function(response) {
                if (chrome.runtime.lastError) {
                  reject(chrome.runtime.lastError.message);
                }
                resolve(response);
              });
        }).catch(throwResultError);
        checkResponse(response, message, isApp);
      }

      async function canConnectAndSendMessages() {
        var port = chrome.runtime.connect(extensionId);
        return new Promise((resolve) => {
          port.postMessage(message);
          port.postMessage(message);
          var pendingResponses = 2;
          port.onMessage.addListener(function(response) {
            pendingResponses--;
            checkResponse(response, message, isApp);
            if (pendingResponses == 0) {
              return resolve(results.OK);
            }
          });
        });
      }

      await canSendMessage();
      return await canConnectAndSendMessages();
    } catch (err) {
      if (err instanceof ResultError) {
        return err.result;
      }
      throw err;
    }
  },

  canUseSendMessagePromise: async function(extensionId, isApp) {
    try {
      const response = await chrome.runtime.sendMessage(extensionId, kMessage);
      checkResponse(response, kMessage, isApp);
      return results.OK;
    } catch (error) {
      if (error instanceof ResultError) {
        return error.result;
      }
      throw error;
    }
  },

  trySendMessage: function(extensionId) {
    chrome.runtime.sendMessage(extensionId, kMessage, function(response) {
      // The result is unimportant. All that matters is the attempt.
    });
  },

  tryIllegalArguments: function() {
    // Tests that illegal arguments to messaging functions throw exceptions.
    // Regression test for crbug.com/472700, where they crashed the renderer.
    function runIllegalFunction(fun) {
      try {
        fun();
      } catch (e) {
        return true;
      }
      console.error('Function did not throw exception: ' + fun);
      return false;
    }
    return runIllegalFunction(chrome.runtime.connect) &&
        runIllegalFunction(function() {
          chrome.runtime.connect('');
        }) &&
        runIllegalFunction(function() {
          chrome.runtime.connect(42);
        }) &&
        runIllegalFunction(function() {
          chrome.runtime.connect('', 42);
        }) &&
        runIllegalFunction(function() {
          chrome.runtime.connect({name: 'noname'});
        }) &&
        runIllegalFunction(chrome.runtime.sendMessage) &&
        runIllegalFunction(function() {
          chrome.runtime.sendMessage('');
        }) &&
        runIllegalFunction(function() {
          chrome.runtime.sendMessage(42);
        }) &&
        runIllegalFunction(function() {
          chrome.runtime.sendMessage('', 42);
        });
  },

  areAnyRuntimePropertiesDefined: function(names) {
    var result = false;
    if (chrome.runtime) {
      forEach.call(names, function(name) {
        if (chrome.runtime[name]) {
          console.log('runtime.' + name + ' is defined');
          result = true;
        }
      });
    }
    return result;
  },

  getTlsChannelIdFromPortConnect: function(extensionId, includeTlsChannelId,
                                           message) {
    try {
      checkRuntime();
    } catch (err) {
      if (err instanceof ResultError) {
        return sendToBrowserForTlsChannelId(err.result);
      }
      throw err;
    }

    if (!message)
      message = kMessage;

    var port = chrome.runtime.connect(extensionId,
        {'includeTlsChannelId': includeTlsChannelId});
    return new Promise(resolve => {
      port.onMessage.addListener(resolve);
      port.postMessage(message);
    }).then(checkTlsChannelIdResponse);
  },

  getTlsChannelIdFromSendMessage: function(extensionId, includeTlsChannelId,
                                           message) {
    try {
      checkRuntime();
    } catch (err) {
      if (err instanceof ResultError) {
        return sendToBrowserForTlsChannelId(err.result);
      }
      throw err;
    }

    if (!message)
      message = kMessage;

    return new Promise(resolve => {
      chrome.runtime.sendMessage(extensionId, message,
          {'includeTlsChannelId': includeTlsChannelId},
          resolve);
    }).then(checkTlsChannelIdResponse);
  }
};

}());
