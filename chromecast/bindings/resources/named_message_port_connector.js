// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (!cast)
  var cast = new Object;

if (!cast.__platform__)
  cast.__platform__ = new Object;

// Creates named HTML5 MessagePorts that are connected to native code.
cast.__platform__.PortConnector = new class {
  constructor() {
    this.controlPort_ = null;

    // A map of ports waiting to be published to the controlPort_, keyed by
    // string IDs.
    this.pendingPorts_ = {};

    this.listener = this.onMessageEvent.bind(this);
    window.addEventListener(
        'message',
        this.listener,
        true  // Let the listener handle events before they hit the DOM tree.
        );
  }

  // Returns a MessagePort whose channel will be passed to the native code.
  // The channel can be used immediately after construction. Outgoing messages
  // will be automatically buffered until the connection is established.
  bind(id) {
    var channel = new MessageChannel();
    if (this.controlPort_)
      this.sendPort(id, channel.port2);
    else
      this.pendingPorts_[id] = channel.port2;

    return channel.port1;
  }

  sendPort(portId, port) {
    this.controlPort_.postMessage(portId, [port]);
  }

  // Receives a control port from native code.
  onMessageEvent(e) {
    // Only process window.onmessage events which are intended for this class.
    if (e.data != 'cast.master.connect')
      return;

    if (e.ports.length != 1) {
      console.error('Expected only one MessagePort, got ' + e.ports.length +
                    ' instead.');

      for (var i in e.ports)
        e.ports[i].close()

      return;
    }

    this.controlPort_ = e.ports[0]
    for (var portId in this.pendingPorts_) {
      this.sendPort(portId, this.pendingPorts_[portId]);
    }
    this.pendingPorts_ = null;

    e.stopPropagation();

    // No need to receive more onmessage events.
    window.removeEventListener('message', this.listener);
  }
}();
