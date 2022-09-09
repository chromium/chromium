// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Messaging = {};

// Structure of a simple message sent/received using |window.postMessage| and
// |window.onmessage|.
Messaging.Message = function (title, content) {
  this.title = title;
  for (var key in content) {
    if (!!content[key]) {
      this[key] = content[key];
    }
  }
};

// A Singleton class which handles received messages and dispatches them to the
// registered handlers. Dispatching is based on |title|.
Messaging.GetHandler = function() {
  if (arguments.callee._singletonInstance) {
    return arguments.callee._singletonInstance;
  }
  arguments.callee._singletonInstance = this;
  var handlers = {};

  this.addHandler = function(name, handlerFunction) {
    handlers[name] = handlerFunction;
  };

  this.removeHandler = function(name) {
    handlers[name] = null;
  };

  this.sendMessage = function(message, portTo) {
    portTo.postMessage(message, '*');
  };

  function onMessage(msg) {
    var message = msg.data;
    var from = msg.source;
    console.log('Received message "' + JSON.stringify(message) + '".');
    if (!handlers[message.title]) {
      console.log('Title "' + message.title + '" is not handled.');
    } else {
      handlers[message.title](message, from);
    }
  }
  window.onmessage = onMessage;
  return arguments.callee._singletonInstance;
};
