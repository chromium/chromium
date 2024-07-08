// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LensSidePanelPageCallbackRouter, type LensSidePanelPageHandlerInterface, type LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import type {SidePanelBrowserProxy} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the LensSidePanelPageHandler used to verify calls to the
 * browser from WebUI.
 */
export class TestLensSidePanelPageHandler extends TestBrowserProxy implements
    LensSidePanelPageHandlerInterface {
  constructor() {
    super([
      'popAndLoadQueryFromHistory',
    ]);
  }

  popAndLoadQueryFromHistory() {
    this.methodCalled('popAndLoadQueryFromHistory');
  }
}

/**
 * Test version of the BrowserProxy used in connecting Lens Side Panel to the
 * browser on start up.
 */
export class TestLensSidePanelBrowserProxy extends TestBrowserProxy implements
    SidePanelBrowserProxy {
  callbackRouter: LensSidePanelPageCallbackRouter =
      new LensSidePanelPageCallbackRouter();
  handler: TestLensSidePanelPageHandler = new TestLensSidePanelPageHandler();
  page: LensSidePanelPageRemote =
      this.callbackRouter.$.bindNewPipeAndPassRemote();
}
