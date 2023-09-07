// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GetOauthTokenStatus, ParentAccessParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {ParentAccessUiHandlerInterface} */
export class TestParentAccessUiHandler extends TestBrowserProxy {
  constructor() {
    super([
      'getOauthToken',
      'onParentAccessCallbackReceived',
      'getParentAccessParams',
      'getParentAccessUrl',
      'onParentAccessDone',
      'onBeforeScreenDone',
    ]);

    /** @private {?ParentAccessParams} */
    this.params_ = null;

    /** @private {?string} */
    this.oAuthToken_ = null;

    /** @private {?GetOauthTokenStatus} */
    this.oAuthTokenStatus_ = null;
  }

  /** @override */
  getOauthToken() {
    this.methodCalled('getOauthToken');
    return Promise.resolve({
      oauthToken: this.oAuthToken_,
      status: this.oAuthTokenStatus_,
    });
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
  getParentAccessUrl() {
    this.methodCalled('getParentAccessUrl');
    return Promise.resolve({url: 'https://families.google.com/parentaccess'});
  }

  /** @override */
  onParentAccessDone(parentAccessResult) {
    this.methodCalled('onParentAccessDone', parentAccessResult);
  }

  /** @override */
  onBeforeScreenDone() {
    this.methodCalled('onBeforeScreenDone');
  }

  /**
   * @param {!ParentAccessParams} params
   */
  setParentAccessParams(params) {
    this.params_ = params;
  }

  /**
   * @param {string} token
   * @param {!GetOauthTokenStatus} status
   */
  setOauthTokenStatus(token, status) {
    this.oAuthToken_ = token;
    this.oAuthTokenStatus_ = status;
  }
}
