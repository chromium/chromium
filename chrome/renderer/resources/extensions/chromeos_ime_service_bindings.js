// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('ash.ime.mojom.ime_service.mojom');
loadScript('ash.ime.mojom.input_engine.mojom');
loadScript('ash.ime.mojom.input_method.mojom');
loadScript('ash.ime.mojom.input_method_host.mojom');

/**
 * Empty result to keep Mojo pipe from disconnection.
 * @type {Promise}
 * @const
 */
var IME_CHANNEL_EMPTY_RESULT = Promise.resolve({result: ""});

/**
 * Empty message to keep Mojo pipe from disconnection.
 * @type {Uint8Array}
 * @const
 */
var IME_CHANNEL_EMPTY_EXTRA = new Uint8Array(0);

/*
 * Represents the js-side of the InputChannel.
 * Routes calls from IME service to the IME extension.
 * @implements {ash.ime.mojom.InputChannel}
 */
class ImeExtensionChannel {
  constructor() {
    /**
     * @private @const
     * @type {!mojo.Binding}
     * */
    this.binding_ = new mojo.Binding(ash.ime.mojom.InputChannel, this);

    /**
     * @private
     * @type {ash.ime.mojom.InputChannelPtr}
     */
    this.channelPtr_ = undefined;

    /**
     * Handler for the text message.
     *
     * @private
     * @type {function(string):string}
     */
    this.textHandler_ = undefined;

    /**
     * Handler for the protobuf message.
     *
     * @private
     * @type {function(Uint8Array):Uint8Array}
     */
    this.protobufHandler_ = undefined;
  }

  /**
   * Get a cached bound InterfacePtr for this InputChannel impl.
   * Create one the ptr if it's not bound yet.
   *
   * @return {!ash.ime.mojom.InputChannelPtr}.
   */
  getChannelPtr() {
    return this.binding_.createInterfacePtrAndBind()
  }

  /**
   * Set a handler for processing text message. The handler must return a
   * nonnull string, otherwise it will lead to disconnection.
   *
   * @param {function(string):string} handler.
   */
  onTextMessage(handler) {
    this.textHandler_ = handler;
    return this;
  }

  /**
   * Set a handler for processing protobuf message. The handler must return a
   * nonnull Uint8Array, otherwise it will lead to disconnection.
   *
   * @param {function(!Uint8Array):!Uint8Array} handler.
   */
  onProtobufMessage(handler) {
    this.protobufHandler_ = handler;
    return this;
  }

  /**
   * Process the text message from a connected input engine.
   *
   * @type {function(string):Promise<string>}
   * @private
   * @param {string} message
   * @return {!Promise<string>} result.
   */
  processText(message) {
    if (this.textHandler_) {
      return Promise.resolve({result: this.textHandler_(message)});
    }
    return IME_CHANNEL_EMPTY_RESULT;
  }

  /**
   * Process the protobuf message from a connected input engine.
   *
   * @type {function(Uint8Array):Promise<Uint8Array>}
   * @private
   * @param {!Uint8Array} message
   * @return {!Promise<!Uint8Array>}
   */
  processMessage(message) {
    if (this.protobufHandler_) {
      return Promise.resolve({result: this.protobufHandler_(message)});
    }
    return IME_CHANNEL_EMPTY_RESULT;
  }

  /**
   * Set the error handler when the channel Mojo pipe is disconnected.
   *
   * @param {function():void} handler.
   */
  setConnectionErrorHandler(handler) {
    if (handler) {
      this.binding_.setConnectionErrorHandler(handler);
    }
  }
}

/*
 * The main entry point to the IME Mojo service.
 */
class ImeService {
  /** @param {!ash.ime.mojom.InputEngineManagerPtr} */
  constructor(manager) {
    /**
     * The IME Mojo service. Allows extension code to fetch an engine instance
     * implemented in the connected IME service.
     * @private
     * @type {!ash.ime.mojom.InputEngineManagerPtr}
     */
    this.manager_ = manager;

    /**
     * TODO(crbug.com/837156): Build KeepAlive Mojo pipe.
     * Handle to a KeepAlive service object, which prevents the extension from
     * being suspended as long as it remains in scope.
     * @private
     * @type {boolean}
     */
    this.keepAlive_ = null;

    /**
     * An active IME Engine proxy. Allows extension code to make calls on the
     * connected InputEngine that resides in the IME service.
     * @private
     * @type {!ash.ime.mojom.InputChannelPtr}
     */
    this.activeEngine_ = null;

    /**
     * A to-client channel instance to receive data from the connected Engine
     * that resides in the IME service.
     * @private
     * @type {ImeExtensionChannel}
     */
    this.clientChannel_ = null;
  }

  /** @return {boolean} True if there is a connected IME service. */
  isConnected() {
    return this.manager_ && this.manager_.ptr.isBound();
  }

  /**
   * Set the error handler when the IME Mojo service is disconnected.
   *
   * @param {function():void} callback.
   */
  setConnectionErrorHandler(callback) {
    if (callback && this.isConnected()) {
      this.manager_.ptr.setConnectionErrorHandler(callback);
    }
  }

  /**
   * @return {?ash.ime.mojom.InputChannelPtr} A bound IME engine instance
   * or null if no IME Engine is bound.
   */
  getActiveEngine() {
    if (this.activeEngine_ && this.activeEngine_.ptr.isBound()) {
      return this.activeEngine_;
    }
    return null;
  }

  /**
   * Set a handler for the client delegate to process plain text messages.
   *
   * @param {!function(string):string} callback Callback on text message.
   */
  setDelegateTextHandler(callback) {
    if (this.clientChannel_) {
      this.clientChannel_.onTextMessage(callback);
    }
  }

  /**
   * Set a handler for the client delegate to process protobuf messages.
   *
   * @param {!function(!Uint8Array):!Uint8Array} callback Callback on protobuf
   *     message.
   */
  setDelegateProtobufHandler(callback) {
    if (this.clientChannel_) {
      this.clientChannel_.onProtobufMessage(callback);
    }
  }

  /**
   * Activates an input method based on its specification.
   *
   * @param {string} imeSpec The specification of an IME (e.g. the engine ID).
   * @param {!Uint8Array} extra The extra data (e.g. initial tasks to run).
   * @param {function(boolean):void} onConnection The callback function to
   *     invoke when the IME activation is done.
   * @param {function():void} onConnectionError The callback function to
   *     invoke when the Mojo pipe on the active engine is disconnected.
   */
  activateIME(imeSpec, extra, onConnection, onConnectionError) {
    if (this.isConnected()) {

      // TODO(crbug.com/837156): Try to reuse the current engine if possible.
      // Disconnect the current active engine and make a new one.
      this.deactivateIME();
      this.activeEngine_ = new ash.ime.mojom.InputChannelPtr;

      // Null value will cause a disconnection on the Mojo pipe.
      extra = extra ? extra : IME_CHANNEL_EMPTY_EXTRA;

      // Create a client side channel to receive data from service.
      if (!this.clientChannel_) {
        this.clientChannel_ = new ImeExtensionChannel();
      }

      this.manager_
          .connectToImeEngine(
              imeSpec, mojo.makeRequest(this.activeEngine_),
              this.clientChannel_.getChannelPtr(), extra)
          .then((result) => {
            const bound = result && result['success'];
            if (bound && onConnectionError) {
              this.activeEngine_.ptr.setConnectionErrorHandler(
                  onConnectionError);
            };
            if (onConnection) {
              onConnection(bound);
            };
          });
    }
  }

  /** Deactivate the IME engine if it is connected. */
  deactivateIME() {
    if (this.getActiveEngine()) {
      this.activeEngine_.ptr.reset();
    }
    this.activeEngine_ = null;
    // TODO(crbug.com/837156): Release client channel?
  }
}

(function() {
  let ptr = new ash.ime.mojom.InputEngineManagerPtr;
  Mojo.bindInterface(
      ash.ime.mojom.InputEngineManager.name, mojo.makeRequest(ptr).handle);
  exports.$set('returnValue', new ImeService(ptr));
})();
