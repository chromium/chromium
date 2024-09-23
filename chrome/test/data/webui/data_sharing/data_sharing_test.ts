// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://data-sharing/data_sharing_app.js';

import type {BrowserProxy} from 'chrome-untrusted://data-sharing/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome-untrusted://data-sharing/browser_proxy.js';
import type {PageRemote} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {PageCallbackRouter} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {DataSharingApp} from 'chrome-untrusted://data-sharing/data_sharing_app.js';
import {Code} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import {DataSharingSdkImpl} from 'chrome-untrusted://data-sharing/dummy_data_sharing_sdk.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome-untrusted://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

class TestDataSharingBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;

  constructor() {
    super([
      'showUi',
      'closeUi',
    ]);
    this.callbackRouter = new PageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  showUi() {
    this.methodCalled('showUi');
  }

  closeUi(status: Code) {
    this.methodCalled('closeUi', status);
  }
}

suite('Start flows', () => {
  let testDataSharingSdk: TestMock<DataSharingSdkImpl>&DataSharingSdkImpl;
  let dataSharingApp: DataSharingApp|null = null;
  let testBrowserProxy: TestDataSharingBrowserProxy;

  setup(async () => {
    testBrowserProxy = new TestDataSharingBrowserProxy();
    testDataSharingSdk = TestMock.fromClass(DataSharingSdkImpl);
    DataSharingSdkImpl.setInstance(testDataSharingSdk);
    BrowserProxyImpl.setInstance(testBrowserProxy);
    testDataSharingSdk.setResultFor(
        'runInviteFlow', Promise.resolve({status: Code.OK}));
    testDataSharingSdk.setResultFor(
        'runJoinFlow', Promise.resolve({status: Code.OK}));
    testDataSharingSdk.setResultFor(
        'runManageFlow', Promise.resolve({status: Code.OK}));
  });

  test('Invite flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=share&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runInviteFlow'));
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Manage flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=manage&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runManageFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runManageFlow')[0].groupId);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Join flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runJoinFlow'));
    const arg = testDataSharingSdk.getArgs('runJoinFlow')[0];
    assertEquals('fake_group_id', arg.groupId);
    assertEquals('fake_token', arg.tokenSecret);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });
});
