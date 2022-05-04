// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote} from 'chrome://read-later.top-chrome/read_anything/read_anything.mojom-webui.js';
import {ReadAnythingApiProxy} from 'chrome://read-later.top-chrome/read_anything/read_anything_api_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestReadAnythingApiProxy extends TestBrowserProxy implements
    ReadAnythingApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;

  constructor() {
    super([
      'onUIReady',
    ]);

    this.callbackRouter = new PageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  getCallbackRouterRemote(): PageRemote {
    return this.callbackRouterRemote;
  }

  onUIReady() {
    this.methodCalled('onUIReady');
  }
}
