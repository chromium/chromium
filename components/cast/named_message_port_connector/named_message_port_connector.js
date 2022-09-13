// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (!window['cast']) {
  /**
   * @const
   */
  // eslint-disable-next-line no-var
  var cast = new Object();
}

if (!cast['__platform__']) {
  /**
   * @const
   */
  cast.__platform__ = {};
}

// Creates named HTML5 MessagePorts that are connected to native code.
cast.__platform__.PortConnector = new class {
  constructor() {
    /** @private {MessagePort} */
    this.controlPort_ = null;

    // A map of ports waiting to be published to the controlPort_, keyed by
    // string IDs.
    /** @private {Object<string, MessagePort>} */
    this.pendingPorts_ = {};

    /** @private */
    this.listener = this.onMessageEvent.bind(this);

    window.addEventListener(
        'message',
        this.listener,
        true,  // Let the listener handle events before they hit the DOM tree.
    );
  }

  /**
   * Returns a MessagePort whose channel will be passed to the native code.
   * The channel can be used immediately after construction. Outgoing messages
   * will be automatically buffered until the connection is established.
   * @param {string} id The ID of the port being registered.
   * @return {MessagePort}
   */
  bind(id) {
    const channel = new MessageChannel();
    if (this.controlPort_) {
      this.sendPort(id, channel.port2);
    } else {
      this.pendingPorts_[id] = channel.port2;
    }

    return channel.port1;
  }

  /**
   * Sends a MessagePort to the remote NamedMessagePortConnector.
   * @param {string} portId The name of the port to send over the control port.
   * @param {MessagePort} port The port being sent.
   */
  sendPort(portId, port) {
    this.controlPort_.postMessage(portId, [port]);
  }

  /**
   * Handles frame message events to receive a connection "control port" from
   * native code.
   * @param {Event} messageEvent
   */
  onMessageEvent(messageEvent) {
    // Only process window.onmessage events which are intended for this class.
    if (messageEvent.data != 'cast.master.connect') {
      return;
    }

    if (messageEvent.ports.length != 1) {
      console.error(
          'Expected only one MessagePort, got ' + messageEvent.ports.length +
          ' instead.');
      for (const port of messageEvent.ports) {
        port.close();
      }
      return;
    }

    this.controlPort_ = messageEvent.ports[0];
    for (const portId in this.pendingPorts_) {
      this.sendPort(portId, this.pendingPorts_[portId]);
    }
    this.pendingPorts_ = null;

    messageEvent.stopPropagation();

    // No need to receive more onmessage events.
    window.removeEventListener('message', this.listener);
  }
}
();
