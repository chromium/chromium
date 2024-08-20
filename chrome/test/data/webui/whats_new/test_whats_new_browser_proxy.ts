// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {JSTime, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {PageCallbackRouter} from 'chrome://whats-new/whats_new.mojom-webui.js';
import type {ModulePosition, PageHandlerInterface, ScrollDepth} from 'chrome://whats-new/whats_new.mojom-webui.js';
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
      'recordTimeToLoadContent',
      'recordVersionPageLoaded',
      'recordEditionPageLoaded',
      'recordModuleImpression',
      'recordExploreMoreToggled',
      'recordScrollDepth',
      'recordTimeOnPage',
      'recordModuleLinkClicked',
      'recordBrowserCommandExecuted',
    ]);

    this.url_ = {url};
  }

  getServerUrl() {
    this.methodCalled('getServerUrl');
    return Promise.resolve({url: this.url_});
  }

  recordTimeToLoadContent(time: JSTime) {
    this.methodCalled('recordTimeToLoadContent', time);
  }

  recordVersionPageLoaded(isAutoOpen: boolean) {
    this.methodCalled('recordVersionPageLoaded', isAutoOpen);
  }

  recordEditionPageLoaded(pageUid: string, isAutoOpen: boolean) {
    this.methodCalled('recordEditionPageLoaded', pageUid, isAutoOpen);
  }

  recordModuleImpression(moduleName: string, position: ModulePosition) {
    this.methodCalled('recordModuleImpression', moduleName, position);
  }

  recordExploreMoreToggled(expanded: boolean) {
    this.methodCalled('recordExploreMoreToggled', expanded);
  }

  recordScrollDepth(percent: ScrollDepth) {
    this.methodCalled('recordScrollDepth', percent);
  }

  recordTimeOnPage(time: TimeDelta) {
    this.methodCalled('recordTimeOnPage', time);
  }

  recordModuleLinkClicked(moduleName: string, position: ModulePosition) {
    this.methodCalled('recordModuleLinkClicked', moduleName, position);
  }

  recordBrowserCommandExecuted() {
    this.methodCalled('recordBrowserCommandExecuted');
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
