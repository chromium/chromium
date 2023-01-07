// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings_lite');
}

loadScript('ash.enhanced_network_tts.mojom-lite');

class EnhancedNetworkTtsAdapter {
  constructor() {
    /**
     * @private {!ash.enhancedNetworkTts.mojom.EnhancedNetworkTtsRemote} the
     *     remote for the enhanced network tts.
     */
    this.enhancedNetworkTts_ =
        ash.enhancedNetworkTts.mojom.EnhancedNetworkTts.getRemote();
  }

  /**
   * The max length for the utterance in a |TtsRequest|.
   * @return {!number}
   */
  get MAX_CHARACTER_SIZE() {
    return ash.enhancedNetworkTts.mojom.ENHANCED_NETWORK_TTS_MAX_CHARACTER_SIZE;
  }

  /**
   * Gets the Text-to-Speech data for the |request|. The data is generated from
   * a Google API that uses enhanced voices.
   * @param {!ash.enhancedNetworkTts.mojom.TtsRequest} request
   * @param {function(!ash.enhancedNetworkTts.mojom.TtsResponse): void} callback
   *   a function that receives TtsResponse.
   * @return {!Promise<void>}
   */
  async getAudioDataWithCallback(request, callback) {
    const pending_receiver =
        (await this.enhancedNetworkTts_.getAudioData(request)).observer;
    this.callbackRouter_ =
        new ash.enhancedNetworkTts.mojom.AudioDataObserverCallbackRouter();
    this.callbackRouter_.$.bindHandle(pending_receiver.handle);
    return this.callbackRouter_.onAudioDataReceived.addListener(callback);
  }

  /**
   * Removes prior established binding and reset the receiver.
   */
  resetApi() {
    if (this.callbackRouter_ && this.callbackRouter_.$) {
      this.callbackRouter_.$.close();
    }
  }
}

exports.$set('returnValue', new EnhancedNetworkTtsAdapter());
