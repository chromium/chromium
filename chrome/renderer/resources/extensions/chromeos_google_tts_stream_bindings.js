// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('chromeos.tts.mojom.google_tts_stream.mojom');

(function() {
  let ptr = new chromeos.tts.mojom.GoogleTtsStreamPtr;
  Mojo.bindInterface(
      chromeos.tts.mojom.GoogleTtsStream.name, mojo.makeRequest(ptr).handle);
  exports.$set('returnValue', ptr);
})();
