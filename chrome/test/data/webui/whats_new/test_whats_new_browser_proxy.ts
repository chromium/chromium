// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {PageCallbackRouter} from 'chrome://whats-new/whats_new.mojom-webui.js';
import type {PageHandlerInterface} from 'chrome://whats-new/whats_new.mojom-webui.js';
import type {WhatsNewProxy} from 'chrome://whats-new/whats_new_proxy.js';

/**
 * Test version of the WhatsNewPageHandler used to verify calls to the
 * browser from WebUI.
 */
class TestWhatsNewPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private url_: Url;

  constructor(url: string) {
    super([
      'getServerUrl',
    ]);

    this.url_ = {url};
  }

  getServerUrl() {
    this.methodCalled('getServerUrl');
    return Promise.resolve({url: this.url_});
  }
}

/**
 * Test version of the BrowserProxy used in connecting the What's New
 * page to the browser on start up.
 */
export class TestWhatsNewBrowserProxy extends TestBrowserProxy implements
    WhatsNewProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestWhatsNewPageHandler;

  /**
   * @param url The URL to load in the iframe.
   */
  constructor(url: string) {
    super([]);
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new TestWhatsNewPageHandler(url);
  }
}
