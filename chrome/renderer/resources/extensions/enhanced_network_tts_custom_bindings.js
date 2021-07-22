// Copyright 2021 The Chromium Authors. All rights reserved.
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
   * @return {!Promise<{response: !ash.enhancedNetworkTts.mojom.TtsResponse}>}
   *     the response containing the Text-to-Speech data if the request proceeds
   *     successfully, or an error code if the request fails.
   */
  getAudioData(request) {
    return this.enhancedNetworkTts_.getAudioData(request);
  }
}

exports.$set('returnValue', new EnhancedNetworkTtsAdapter());
