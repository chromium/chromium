// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

// Helper to safely retrieve and bind a prototype getter, or fallback to direct
// property access.
/**
 * @param {!Object} proto
 * @param {string} prop
 * @return {!Function}
 */
function safeGetGetter(proto, prop) {
  try {
    const desc = Object.getOwnPropertyDescriptor(proto, prop);
    if (desc && desc.get) {
      const boundGetter = Function.prototype.call.bind(desc.get);
      return (obj) => {
        try {
          return obj ? boundGetter(obj) : undefined;
        } catch (e) {
          return undefined;
        }
      };
    }
  } catch (e) {
  }
  return (obj) => {
    try {
      return obj ? obj[prop] : undefined;
    } catch (e) {
      return undefined;
    }
  };
}

// Helper to safely retrieve and bind a prototype method, or fallback to direct
// method call.
/**
 * @param {!Object} proto
 * @param {string} name
 * @return {!Function}
 */
function safeGetMethod(proto, name) {
  try {
    const method = proto[name];
    if (typeof method === 'function') {
      const boundMethod = Function.prototype.call.bind(method);
      return (obj, ...args) => {
        try {
          return obj ? boundMethod(obj, ...args) : undefined;
        } catch (e) {
          return undefined;
        }
      };
    }
  } catch (e) {
  }
  return (obj, ...args) => {
    try {
      return obj ? obj[name](...args) : undefined;
    } catch (e) {
      return undefined;
    }
  };
}

// Cache the native prototype getters and methods immediately at script
// execution time. This prevents subsequent page scripts from overriding
// properties or methods on the prototype chains of events we process.
// We bind Function.prototype.call to the getters/methods so they can be
// securely invoked as standalone functions without relying on the target
// object's prototype.
const getIsTrusted = safeGetGetter(Event.prototype, 'isTrusted');
const stopPropagation = safeGetMethod(Event.prototype, 'stopPropagation');

const getSource = safeGetGetter(MessageEvent.prototype, 'source');
const getOrigin = safeGetGetter(MessageEvent.prototype, 'origin');
const getData = safeGetGetter(MessageEvent.prototype, 'data');
const getPorts = safeGetGetter(MessageEvent.prototype, 'ports');

const addEventListener =
    safeGetMethod(EventTarget.prototype, 'addEventListener');
const removeEventListener =
    safeGetMethod(EventTarget.prototype, 'removeEventListener');
const SafeMessageChannel = window.MessageChannel;
const getPort1 = safeGetGetter(MessageChannel.prototype, 'port1');
const getPort2 = safeGetGetter(MessageChannel.prototype, 'port2');
const postMessage = safeGetMethod(MessagePort.prototype, 'postMessage');
const closePort = safeGetMethod(MessagePort.prototype, 'close');
const safeObjectCreate = Object.create;
const safeDefineProperty = Object.defineProperty;
const safeGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
const safeOwnKeys = Reflect.ownKeys;
const arrayIncludes = safeGetMethod(Array.prototype, 'includes');


// Reuse existing namespaces if they exist to maintain compatibility with other
// scripts.
const castNamespace = /** @type {!Object} */ (
    (window.cast && typeof window.cast === 'object') ? window.cast : {});
const platformNamespace = /** @type {!Object} */ (
    (castNamespace.__platform__ &&
     typeof castNamespace.__platform__ === 'object') ?
        castNamespace.__platform__ :
        {});

// Lock down cast.__platform__ to prevent deletion or redefinition.
if (safeGetOwnPropertyDescriptor(castNamespace, '__platform__')
        ?.configurable !== false) {
  safeDefineProperty(castNamespace, '__platform__', {
    value: platformNamespace,
    writable: false,
    configurable: false,
    enumerable: true,
  });
}

// Define window.cast with a custom getter/setter merger.
// This prevents strict-mode errors on common initialization patterns like
// 'window.cast = window.cast || {}' while emulating property overwrites if
// the page script tries to completely replace the object.
// The '__platform__' property is explicitly preserved across reassignments to
// protect internal platform bindings from deletion or modification.
if (safeGetOwnPropertyDescriptor(window, 'cast')?.configurable !== false) {
  safeDefineProperty(window, 'cast', {
    get() {
      return castNamespace;
    },
    set(val) {
      if (val && typeof val === 'object' && val !== castNamespace) {
        // Emulate overwrite by deleting old properties not present in the new
        // value (excluding __platform__).
        const newKeys = safeOwnKeys(/** @type {!Object} */ (val));
        const oldKeys = safeOwnKeys(castNamespace);
        for (let i = 0; i < oldKeys.length; i++) {
          const key = oldKeys[i];
          if (key === '__platform__') {
            continue;
          }
          if (!arrayIncludes(newKeys, key)) {
            try {
              delete castNamespace[key];
            } catch (e) {
            }
          }
        }

        // Copy new properties onto the persistent namespace object (excluding
        // __platform__).
        for (let i = 0; i < newKeys.length; i++) {
          const key = newKeys[i];
          if (key === '__platform__') {
            continue;
          }
          try {
            const desc =
                safeGetOwnPropertyDescriptor(/** @type {!Object} */ (val), key);
            if (desc) {
              safeDefineProperty(
                  castNamespace,
                  key,
                  /** @type {!Object} */ (desc),
              );
            }
          } catch (e) {
          }
        }
      }
    },
    configurable: false,
    enumerable: true,
  });
}

let listener;

// Creates named HTML5 MessagePorts that are connected to native code.
// Instantiated locally first; locked down on the namespace at the bottom of the
// script.
const portConnectorInstance = new class {
  constructor() {
    /** @private {MessagePort} */
    this.controlPort_ = null;

    // A map of ports waiting to be published to the controlPort_, keyed by
    // string IDs.
    /** @private {Object<string, MessagePort>} */
    this.pendingPorts_ = safeObjectCreate(null);

    listener = this.onMessageEvent.bind(this);

    addEventListener(
        window,
        'message',
        listener,
        true,
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
    // Use the securely cached MessageChannel constructor to prevent
    // interception.
    const channel = new SafeMessageChannel();
    const port1 = /** @type {!MessagePort} */ (getPort1(channel));
    const port2 = /** @type {!MessagePort} */ (getPort2(channel));
    if (this.controlPort_) {
      this.sendPort(id, port2);
    } else {
      this.pendingPorts_[id] = port2;
    }

    return port1;
  }

  /**
   * Sends a MessagePort to the remote NamedMessagePortConnector.
   * @param {string} portId The name of the port to send over the control port.
   * @param {MessagePort} port The port being sent.
   */
  sendPort(portId, port) {
    // Use the securely cached postMessage method to prevent interception.
    postMessage(this.controlPort_, portId, [port]);
  }

  /**
   * Handles frame message events to receive a connection "control port" from
   * native code.
   * @param {Event} messageEvent
   */
  onMessageEvent(messageEvent) {
    // Perform boundary checks using securely cached getters to prevent spoofing
    // via prototype pollution.
    if (!getIsTrusted(messageEvent) || getSource(messageEvent) !== null) {
      return;
    }
    const origin = getOrigin(messageEvent);
    if (origin !== '' && origin !== 'null') {
      return;
    }

    // Only process window.onmessage events which are intended for this class.
    if (getData(messageEvent) !== 'cast.master.connect') {
      return;
    }

    const ports = getPorts(messageEvent);
    if (!ports || ports.length !== 1) {
      console.error(
          'Expected exactly one MessagePort, got ' +
              (ports ? ports.length : 0) + ' instead.',
      );
      if (ports) {
        for (const port of ports) {
          closePort(port);
        }
      }
      return;
    }

    this.controlPort_ = ports[0];
    for (const portId in this.pendingPorts_) {
      this.sendPort(portId, this.pendingPorts_[portId]);
    }
    this.pendingPorts_ = null;

    stopPropagation(messageEvent);

    // No need to receive more onmessage events. Remove listener using the
    // cached method.
    removeEventListener(window, 'message', listener, true);
  }
}
();

// Lock down PortConnector on the platform namespace as read-only and
// non-configurable.
if (safeGetOwnPropertyDescriptor(platformNamespace, 'PortConnector')
        ?.configurable !== false) {
  safeDefineProperty(platformNamespace, 'PortConnector', {
    value: portConnectorInstance,
    writable: false,
    configurable: false,
    enumerable: true,
  });
}
})();
