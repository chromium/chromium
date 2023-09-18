// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GetOauthTokenStatus, ParentAccessParams, ParentAccessResult, ParentAccessServerMessage, ParentAccessUiHandlerInterface} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestParentAccessUiHandler extends TestBrowserProxy implements
    ParentAccessUiHandlerInterface {
  private params: ParentAccessParams|null;
  private oauthToken: string;
  private oauthTokenStatus: GetOauthTokenStatus|null;

  constructor() {
    super([
      'getOauthToken',
      'onParentAccessCallbackReceived',
      'getParentAccessParams',
      'getParentAccessUrl',
      'onParentAccessDone',
      'onBeforeScreenDone',
    ]);

    this.params = null;
    this.oauthToken = '';
    this.oauthTokenStatus = null;
  }


  getOauthToken(): Promise<{oauthToken: string, status: GetOauthTokenStatus}> {
    this.methodCalled('getOauthToken');
    return Promise.resolve({
      oauthToken: this.oauthToken,
      status: this.oauthTokenStatus!,
    });
  }

  onParentAccessCallbackReceived():
      Promise<{message: ParentAccessServerMessage}> {
    this.methodCalled('onParentAccessCallbackReceived');
    return Promise.resolve({message: {type: 0}});
  }

  getParentAccessParams(): Promise<{params: ParentAccessParams}> {
    this.methodCalled('getParentAccessParams');
    return Promise.resolve({params: this.params!});
  }

  getParentAccessUrl(): Promise<{url: string}> {
    this.methodCalled('getParentAccessUrl');
    return Promise.resolve({url: 'https://families.google.com/parentaccess'});
  }

  onParentAccessDone(parentAccessResult: ParentAccessResult): Promise<void> {
    this.methodCalled('onParentAccessDone', parentAccessResult);
    return Promise.resolve();
  }

  onBeforeScreenDone(): Promise<void> {
    this.methodCalled('onBeforeScreenDone');
    return Promise.resolve();
  }

  setParentAccessParams(params: ParentAccessParams) {
    this.params = params;
  }

  setOauthTokenStatus(token: string, status: GetOauthTokenStatus) {
    this.oauthToken = token;
    this.oauthTokenStatus = status;
  }
}
