// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://webnn-internals/webnn_internals.mojom-webui.js';
import type {PageRemote} from 'chrome://webnn-internals/webnn_internals.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestWebnnInternalsBrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  page: PageRemote;

  constructor() {
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}
