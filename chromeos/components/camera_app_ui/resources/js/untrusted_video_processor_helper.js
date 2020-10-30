// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from './lib/comlink.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessorHelperInterface} from './untrusted_helper_interfaces.js';

/**
 * The MP4 video processor URL in trusted type.
 * @type {!TrustedScriptURL}
 */
const mp4VideoProcessorURL = (() => {
  const staticUrlPolicy = trustedTypes.createPolicy(
      'mp4-js-static',
      {createScriptURL: () => '/js/models/mp4_video_processor.js'});
  // TODO(crbug.com/980846): Remove the empty string if
  // https://github.com/w3c/webappsec-trusted-types/issues/278 gets fixed.
  return staticUrlPolicy.createScriptURL('');
})();

/**
 * Connects the |port| to worker which exposes the video processor.
 * @param {!Port} port
 * @return {!Promise}
 */
async function connectToWorker(port) {
  /**
   * Closure Compiler only supports string rather than TrustedScriptURL as
   * parameter to Worker.
   * @suppress {checkTypes}
   */
  const /** string */ trustedURL = mp4VideoProcessorURL;

  const worker = Comlink.wrap(new Worker(trustedURL, {type: 'module'}));
  await worker.exposeVideoProcessor(Comlink.transfer(port, [port]));
}

export /** !VideoProcessorHelperInterface */ {connectToWorker};
