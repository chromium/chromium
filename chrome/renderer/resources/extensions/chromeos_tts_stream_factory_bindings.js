// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('chromeos.tts.mojom.tts_stream_factory.mojom');

(function() {
  let ptr = new chromeos.tts.mojom.TtsStreamFactoryPtr;
  Mojo.bindInterface(
      chromeos.tts.mojom.TtsStreamFactory.name, mojo.makeRequest(ptr).handle);
  exports.$set('returnValue', ptr);
})();
