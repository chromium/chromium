// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://data-sharing/data_sharing_app.js';

import type {BrowserProxy} from 'chrome-untrusted://data-sharing/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome-untrusted://data-sharing/browser_proxy.js';
import type {PageRemote} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {GroupAction, GroupActionProgress, PageCallbackRouter} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {DataSharingApp} from 'chrome-untrusted://data-sharing/data_sharing_app.js';
import {Code, LoggingIntent, Progress} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import type {DataSharingSdkSitePreview, RunInviteFlowParams} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import {DataSharingSdkImpl} from 'chrome-untrusted://data-sharing/dummy_data_sharing_sdk.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome-untrusted://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

class TestDataSharingBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;

  constructor() {
    super([
      'showUi',
      'closeUi',
      'makeTabGroupShared',
      'aboutToUnShareTabGroup',
      'onTabGroupUnShareComplete',
      'getShareLink',
      'getTabGroupPreview',
      'onGroupAction',
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

  makeTabGroupShared(tabGroupId: string, groupId: string, tokenSecret: string):
      Promise<string> {
    this.methodCalled('makeTabGroupShared', [tabGroupId, groupId, tokenSecret]);
    return Promise.resolve('fake_url');
  }

  aboutToUnShareTabGroup(tabGroupId: string) {
    this.methodCalled('aboutToUnShareTabGroup', tabGroupId);
  }

  onTabGroupUnShareComplete(tabGroupId: string) {
    this.methodCalled('onTabGroupUnShareComplete', tabGroupId);
  }

  getShareLink(groupId: string, tokenSecret: string): Promise<string> {
    this.methodCalled('getShareLink', [groupId, tokenSecret]);
    return Promise.resolve('fake_url');
  }

  getTabGroupPreview(groupId: string, tokenSecret: string):
      Promise<DataSharingSdkSitePreview[]> {
    this.methodCalled('getTabGroupPreview', [groupId, tokenSecret]);
    return Promise.resolve([]);
  }

  onGroupAction(action: GroupAction, progress: GroupActionProgress) {
    this.methodCalled('onGroupAction', [action, progress]);
  }
}

suite('Start flows', () => {
  let testDataSharingSdk: TestMock<DataSharingSdkImpl>&DataSharingSdkImpl;
  let dataSharingApp: DataSharingApp|null = null;
  let testBrowserProxy: TestDataSharingBrowserProxy;

  setup(() => {
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
    testDataSharingSdk.setResultFor(
        'runDeleteFlow', Promise.resolve({status: Code.OK}));
    testDataSharingSdk.setResultFor(
        'runCloseFlow', Promise.resolve({status: Code.OK}));
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

  test('Invite flow copy link', async () => {
    // Simulate 2 copy links calls for runInviteFlow.
    testDataSharingSdk.setResultMapperFor(
        'runInviteFlow', async (params: RunInviteFlowParams) => {
          // First getShareLink call will go through makeTabGroupShared().
          await params.getShareLink(
              {groupId: 'fake_group_id', tokenSecret: 'fake_token_secret1'});
          // Subsequent getShareLink call will go through getShareLink().
          await params.getShareLink(
              {groupId: 'fake_group_id', tokenSecret: 'fake_token_secret2'});
          return Promise.resolve({status: Code.OK});
        });

    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=share&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();

    assertEquals(1, testBrowserProxy.getCallCount('makeTabGroupShared'));
    assertEquals(1, testBrowserProxy.getCallCount('getShareLink'));
    assertEquals(1, testDataSharingSdk.getCallCount('runInviteFlow'));
  });

  test('Manage flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=manage&group_id=fake_id&is_disabled_for_policy=true&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runManageFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runManageFlow')[0].groupId);
    assertEquals(
        true, testDataSharingSdk.getArgs('runManageFlow')[0].isSharingDisabled);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Leave flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing/?flow=leave&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.LEAVE_GROUP, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runManageFlow'));
    const arg = testDataSharingSdk.getArgs('runManageFlow')[0];
    assertEquals('fake_id', arg.groupId);
    assertEquals(true, arg.showLeaveDialogAtStartup);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);

    assertEquals(1, testBrowserProxy.getCallCount('onGroupAction'));
    assertDeepEquals(
        [GroupAction.kLeaveGroup, GroupActionProgress.kSuccess],
        testBrowserProxy.getArgs('onGroupAction')[0]);
  });

  test('Delete flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=delete&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.DELETE_GROUP, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runDeleteFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runDeleteFlow')[0].groupId);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);

    assertEquals(1, testBrowserProxy.getCallCount('onGroupAction'));
    assertDeepEquals(
        [GroupAction.kDeleteGroup, GroupActionProgress.kSuccess],
        testBrowserProxy.getArgs('onGroupAction')[0]);
  });

  test('Close flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=close&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runCloseFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runCloseFlow')[0].groupId);
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Join flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.setSuccessfullyJoinedForTesting();
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('showUi'));
    assertEquals(1, testDataSharingSdk.getCallCount('runJoinFlow'));
    const arg = testDataSharingSdk.getArgs('runJoinFlow')[0];
    assertEquals('fake_group_id', arg.groupId);
    assertEquals('fake_token', arg.tokenSecret);
    // Do not close UI if successfully joined because it's supposed to wait
    // until the flow is complete.
    assertEquals(0, testBrowserProxy.getCallCount('closeUi'));
  });

  test('Join flow error case', async () => {
    // If join flows neither joined successfully nor abandon by user, we
    // consider it an error.
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.UNKNOWN, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Join flow abandon case', async () => {
    // If user abandoned join by clicking on the cancel button, we still return
    // Code.OK when the dialog is closed.
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.ABANDON_JOIN, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testBrowserProxy.getCallCount('closeUi'));
    assertEquals(Code.OK, testBrowserProxy.getArgs('closeUi')[0]);
  });

  test('Metrics reporting', async () => {
    loadTimeData.overrideValues({
      metricsReportingEnabled: true,
    });
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=share&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    testBrowserProxy.callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testDataSharingSdk.getCallCount('updateClearcut'));
    const arg = testDataSharingSdk.getArgs('updateClearcut')[0];
    assertEquals(true, arg.enabled);
  });

  test('Load favicon', async () => {
    const img = document.createElement('img');
    img.src =
        'chrome-untrusted://favicon2/?size=16&scaleFactor=1x&pageUrl=chrome%3A%2F%2Fsettings&allowGoogleServerFallback=1';
    document.body.appendChild(img);
    await eventToPromise('load', img);
  });
});
