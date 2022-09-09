// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var socket = chrome.experimental.socket || chrome.socket;

// Stream encapsulates read/write operations over socket.
function Stream(delegate, socketId) {
  this.socketId_ = socketId;
  this.delegate_ = delegate;
}

Stream.prototype = {
  stringToUint8Array_: function(string) {
    var utf8string = unescape(encodeURIComponent(string));
    var buffer = new ArrayBuffer(utf8string.length);
    var view = new Uint8Array(buffer);
    for(var i = 0; i < utf8string.length; i++) {
      view[i] = utf8string.charCodeAt(i);
    }
    return view;
  },

  read: function(callback) {
    socket.read(this.socketId_, function(readInfo) {
      callback(this, readInfo);
    }.bind(this));
  },

  write: function(output) {
    var header = this.stringToUint8Array_(output + '\n\n');
    var outputBuffer = new ArrayBuffer(header.byteLength);
    var view = new Uint8Array(outputBuffer);
    view.set(header, 0);
    socket.write(this.socketId_, outputBuffer, function(writeInfo) {
      if (writeInfo.bytesWritten < 0)
        this.delegate_.onStreamError(this);
    }.bind(this));
  },

  writeError: function(errorCode, description) {
    var content = JSON.stringify({'type': 'error',
                                  'code': errorCode,
                                  'description': description});
    var buffer = this.stringToUint8Array_(content + "\n\n");
    var outputBuffer = new ArrayBuffer(buffer.byteLength);
    var view = new Uint8Array(outputBuffer);
    view.set(buffer, 0);
    socket.write(this.socketId_, outputBuffer, function(writeInfo) {
      this.terminateConnection_();
    }.bind(this));
  },

  terminateConnection_: function() {
    this.delegate_.onStreamTerminated(this);
    socket.destroy(this.socketId_);
  }
};

// Automation server listens socket and passed its processing to
// |connectionHandler|.
function AutomationServer(connectionHandler) {
  this.socketInfo = null;
  this.handler_ = connectionHandler;
}

AutomationServer.instance_ = null;

AutomationServer.getInstance = function() {
  if (!AutomationServer.instance_)
    AutomationServer.instance_ = new AutomationServer(new ConnectionHandler());

  return AutomationServer.instance_;
}

AutomationServer.prototype = {
  onAccept_: function(acceptInfo) {
    console.log("Accepting socket " + acceptInfo.socketId);
    socket.setNoDelay(acceptInfo.socketId, true, function(result) {
      this.handler_.readRequest_(new Stream(this.handler_,
                                            acceptInfo.socketId));
      socket.accept(this.socketInfo.socketId, this.onAccept_.bind(this));
    }.bind(this));
  },

  start: function() {
    socket.create("tcp", {}, function(_socketInfo) {
      this.socketInfo = _socketInfo;
      socket.listen(this.socketInfo.socketId, "127.0.0.1", 8666, 20,
        function(result) {
          console.log("LISTENING:", result);
          socket.accept(this.socketInfo.socketId, this.onAccept_.bind(this));
        }.bind(this));
    }.bind(this));
  }
};
