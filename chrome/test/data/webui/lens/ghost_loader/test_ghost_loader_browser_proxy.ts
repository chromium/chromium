// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LensGhostLoaderPageCallbackRouter, type LensGhostLoaderPageRemote} from 'chrome-untrusted://lens/lens/shared/lens.mojom-webui.js';
import type {BrowserProxy} from 'chrome-untrusted://lens/lens/shared/searchbox_ghost_loader_browser_proxy.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the BrowserProxy used in connecting Lens Ghost Loader to the
 * browser on start up.
 */
export class TestLensGhostLoaderBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: LensGhostLoaderPageCallbackRouter =
      new LensGhostLoaderPageCallbackRouter();
  page: LensGhostLoaderPageRemote =
      this.callbackRouter.$.bindNewPipeAndPassRemote();
}
