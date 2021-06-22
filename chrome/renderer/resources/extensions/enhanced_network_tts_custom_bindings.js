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
   * Gets the audio data for the input |text|. The audio data is generated
   * from a Google API that uses enhanced voices.
   * @param {!string} text
   * @return {!Promise<{bytes: !Array<number>}>} the encoded audio data.
   */
  getAudioData(text) {
    return this.enhancedNetworkTts_.getAudioData(text);
  }
}

exports.$set('returnValue', new EnhancedNetworkTtsAdapter());
