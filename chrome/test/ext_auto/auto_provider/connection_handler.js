// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Automation connection handler is responsible for reading requests from the
// stream, finding and executing appropriate extension API method.
function ConnectionHandler() {
  // Event listener registration map socket->event->callback
  this.eventListener_ = {};
}

ConnectionHandler.prototype = {
  // Stream delegate callback.
  onStreamError: function(stream) {
    this.unregisterListeners_(stream);
  },

  // Stream delegate callback.
  onStreamTerminated: function(stream) {
    this.unregisterListeners_(stream);
  },

  // Pairs event |listenerMethod| with a given |stream|.
  registerListener_: function(stream, eventName, eventObject,
                              listenerMethod) {
    if (!this.eventListener_[stream.socketId_])
      this.eventListener_[stream.socketId_] = {};

    if (!this.eventListener_[stream.socketId_][eventName]) {
      this.eventListener_[stream.socketId_][eventName] = {
          'event': eventObject,
          'method': listenerMethod };
    }
  },

  // Removes event listeners.
  unregisterListeners_: function(stream) {
    if (!this.eventListener_[stream.socketId_])
    return;

    for (var eventName in this.eventListener_[stream.socketId_]) {
      var listenerDefinition = this.eventListener_[stream.socketId_][eventName];
      var removeFunction = listenerDefinition.event['removeListener'];
      if (removeFunction) {
        removeFunction.call(listenerDefinition.event,
                            listenerDefinition.method);
      }
    }
    delete this.eventListener_[stream.socketId_];
  },

  // Finds appropriate method/event to invoke/register.
  findExecutionTarget_: function(functionName) {
    var funcSegments = functionName.split('.');
    if (funcSegments.size < 2)
      return null;

    if (funcSegments[0] != 'chrome')
      return null;

    var eventName = "";
    var prevSegName = null;
    var prevSegment = null;
    var segmentObject = null;
    var segName = null;
    for (var i = 0; i < funcSegments.length; i++) {
      if (prevSegName) {
        if (eventName.length)
          eventName += '.';

        eventName += prevSegName;
      }

      segName = funcSegments[i];
      prevSegName = segName;
      if (!segmentObject) {
        // TODO(zelidrag): Get rid of this eval.
        segmentObject = eval(segName);
        continue;
      }

      prevSegment = segmentObject;
      if (segmentObject[segName])
        segmentObject = segmentObject[segName];
      else
        segmentObject = null;
    }
    if (segmentObject == window)
      return null;

    var isEventMethod = segName == 'addListener';
    return {'method': segmentObject,
            'eventName': (isEventMethod ? eventName : null),
            'event': (isEventMethod ? prevSegment : null)};
  },

  // TODO(zelidrag): Figure out how to automatically detect or generate list of
  // sync API methods.
  isSyncFunction_: function(funcName) {
    if (funcName == 'chrome.omnibox.setDefaultSuggestion')
      return true;

    return false;
  },

  // Parses |command|, finds appropriate JS method runs it with |argsJson|.
  // If the method is an event registration, it will register an event listener
  // method and start sending data from its callback.
  processCommand_: function(stream, command, argsJson) {
    var target = this.findExecutionTarget_(command);
    if (!target || !target.method) {
      return {'result': false,
              'objectName': command};
    }

    var args = JSON.parse(decodeURIComponent(argsJson));
    if (!args)
      args = [];

    console.log(command + '(' + decodeURIComponent(argsJson) + ')',
                stream.socketId_);
    // Check if we need to register an event listener.
    if (target.event) {
      // Register listener method.
      var listener = function() {
        stream.write(JSON.stringify({ 'type': 'eventCallback',
                                      'eventName': target.eventName,
                                      'arguments' : arguments}));
      }.bind(this);
      // Add event handler method to arguments.
      args.push(listener);
      args.push(null);    // for |filters|.
      target.method.apply(target.event, args);
      this.registerListener_(stream, target.eventName,
                             target.event, listener);
      stream.write(JSON.stringify({'type': 'eventRegistration',
                                   'eventName': command}));
      return {'result': true,
              'wasEvent': true};
    }

    // Run extension method directly.
    if (this.isSyncFunction_(command)) {
      // Run sync method.
      console.log(command + '(' + unescape(argsJson) + ')');
      var result = target.method.apply(undefined, args);
      stream.write(JSON.stringify({'type': 'methodResult',
                                   'methodName': command,
                                   'isCallback': false,
                                   'result' : result}));
    } else {    // Async method.
      // Add callback method to arguments.
      args.push(function() {
        stream.write(JSON.stringify({'type': 'methodCallback',
                                     'methodName': command,
                                     'isCallback': true,
                                     'arguments' : arguments}));
      }.bind(this));
      target.method.apply(undefined, args);
    }
    return {'result': true,
            'wasEvent': false};
  },

  arrayBufferToString_: function(buffer) {
    var str = '';
    var uArrayVal = new Uint8Array(buffer);
    for(var s = 0; s < uArrayVal.length; s++) {
      str += String.fromCharCode(uArrayVal[s]);
    }
    return str;
  },

  // Callback for stream read requests.
  onStreamRead_: function(stream, readInfo) {
    console.log("READ", readInfo);
    // Parse the request.
    var data = this.arrayBufferToString_(readInfo.data);
    var spacePos = data.indexOf(" ");
    try {
      if (spacePos == -1) {
        spacePos = data.indexOf("\r\n");
        if (spacePos == -1)
          throw {'code': 400, 'description': 'Bad Request'};
      }

      var verb = data.substring(0, spacePos);
      var isEvent = false;
      switch (verb) {
        case 'TERMINATE':
          throw {'code': 200, 'description': 'OK'};
          break;
        case 'RUN':
          break;
        case 'LISTEN':
          this.isEvent = true;
          break;
        default:
          throw {'code': 400, 'description': 'Bad Request: ' + verb};
          return;
      }

      var command = data.substring(verb.length + 1);
      var endLine = command.indexOf('\r\n');
      if (endLine)
        command = command.substring(0, endLine);

      var objectNames = command;
      var argsJson = null;
      var funcNameEnd =  command.indexOf("?");
      if (funcNameEnd >= 0) {
        objectNames = command.substring(0, funcNameEnd);
        argsJson = command.substring(funcNameEnd + 1);
      }
      var functions = objectNames.split(',');
      for (var i = 0; i < functions.length; i++) {
        var objectName = functions[i];
        var commandStatus =
            this.processCommand_(stream, objectName, argsJson);
        if (!commandStatus.result) {
          throw {'code': 404,
                 'description': 'Not Found: ' + commandStatus.objectName};
        }
        // If we have run all requested commands, read the socket again.
        if (i == (functions.length - 1)) {
          setTimeout(function() {
            this.readRequest_(stream);
          }.bind(this), 0);
        }
      }
    } catch(err) {
      console.warn('Error', err);
      stream.writeError(err.code, err.description);
    }
  },

  // Reads next request from the |stream|.
  readRequest_: function(stream) {
    console.log("Reading socket " + stream.socketId_);
    //  Read in the data
    stream.read(this.onStreamRead_.bind(this));
  }
};
