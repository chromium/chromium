// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {ParentAccessUIHandlerInterface} */
export class TestParentAccessUIHandler extends TestBrowserProxy {
  constructor() {
    super([
      'getOAuthToken',
      'onParentAccessCallbackReceived',
      'getParentAccessParams',
      'getParentAccessURL',
      'onParentAccessDone',
    ]);

    /**
     * @private {?ParentAccessParams}}
     */
    this.params_ = null;
  }

  /** @override */
  getOAuthToken() {
    this.methodCalled('getOAuthToken');
  }

  /** @override */
  onParentAccessCallbackReceived() {
    this.methodCalled('onParentAccessCallbackReceived');
  }

  /** @override */
  getParentAccessParams() {
    this.methodCalled('getParentAccessParams');
    return Promise.resolve({params: this.params_});
  }

  /** @override */
  getParentAccessURL() {
    this.methodCalled('getParentAccessURL');
    return Promise.resolve({url: 'https://families.google.com/parentaccess'});
  }

  /** @override */
  onParentAccessDone(parentAccessResult) {
    this.methodCalled('onParentAccessDone', parentAccessResult);
  }

  /**
   * @param {!ParentAccessParams} params
   */
  setParentAccessParams(params) {
    this.params_ = params;
  }
}
