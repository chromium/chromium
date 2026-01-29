// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxProxyImpl} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter as ComposeboxCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';


type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;


export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer(mock);
  return mock;
}

export class TestSearchboxBrowserProxy extends TestBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouterRemote: PageRemote;

  constructor() {
    super();
    this.callbackRouter = new PageCallbackRouter();
    this.handler = TestMock.fromClass(PageHandlerRemote);
    installMock(
        ComposeboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new ComposeboxCallbackRouter(), this.handler,
            this.callbackRouter)));
    this.handler.setResultFor('getRecentTabs', {tabs: []});
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
  getCallbackRouter() {
    return this.callbackRouter;
  }
}
