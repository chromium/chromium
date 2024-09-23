// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {IssuerTokenCount, PrivateStateTokensApiBrowserProxy, PrivateStateTokensPageHandlerInterface} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivateStateTokensPageHandler extends TestBrowserProxy
    implements PrivateStateTokensPageHandlerInterface {
  privateStateTokensCounts: IssuerTokenCount[] =
      [];  // Initialize as empty array

  constructor() {
    super(['getIssuerTokenCounts']);
  }

  async getIssuerTokenCounts() {
    this.methodCalled('getIssuerTokenCounts');
    return {privateStateTokensCount: this.privateStateTokensCounts};
  }
}

export class TestPrivateStateTokensApiBrowserProxy implements
    PrivateStateTokensApiBrowserProxy {
  handler: TestPrivateStateTokensPageHandler =
      new TestPrivateStateTokensPageHandler();
}
