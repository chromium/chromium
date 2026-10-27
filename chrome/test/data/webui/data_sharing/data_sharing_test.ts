// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://data-sharing/data_sharing_app.js';

import type {GroupPreview, PageHandlerInterface, PageRemote} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {browserProxyFactory, GroupAction, GroupActionProgress} from 'chrome-untrusted://data-sharing/data_sharing.mojom-webui.js';
import {DataSharingApp} from 'chrome-untrusted://data-sharing/data_sharing_app.js';
import {Code, LoggingIntent, Progress} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import type {RunInviteFlowParams} from 'chrome-untrusted://data-sharing/data_sharing_sdk_types.js';
import {DataSharingSdkImpl} from 'chrome-untrusted://data-sharing/dummy_data_sharing_sdk.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {AbslStatusCode} from 'chrome-untrusted://resources/mojo/mojo/public/mojom/base/absl_status.mojom-webui.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome-untrusted://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

class TestDataSharingPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'showUI',
      'closeUI',
      'apiInitComplete',
      'makeTabGroupShared',
      'aboutToUnShareTabGroup',
      'onTabGroupUnShareComplete',
      'getShareLink',
      'getTabGroupPreview',
      'openTabGroup',
      'onGroupAction',
    ]);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  showUI() {
    this.methodCalled('showUI');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  closeUI(statusCode: number) {
    this.methodCalled('closeUI', statusCode);
  }

  apiInitComplete() {
    this.methodCalled('apiInitComplete');
  }

  makeTabGroupShared(tabGroupId: string, groupId: string, accessToken: string):
      Promise<{url: string | null}> {
    this.methodCalled('makeTabGroupShared', [tabGroupId, groupId, accessToken]);
    return Promise.resolve({url: 'fake_url'});
  }

  aboutToUnShareTabGroup(tabGroupId: string) {
    this.methodCalled('aboutToUnShareTabGroup', tabGroupId);
  }

  onTabGroupUnShareComplete(tabGroupId: string) {
    this.methodCalled('onTabGroupUnShareComplete', tabGroupId);
  }

  getShareLink(groupId: string, accessToken: string): Promise<{url: string}> {
    this.methodCalled('getShareLink', [groupId, accessToken]);
    return Promise.resolve({url: 'fake_url'});
  }

  getTabGroupPreview(groupId: string, accessToken: string):
      Promise<{groupPreview: GroupPreview}> {
    this.methodCalled('getTabGroupPreview', [groupId, accessToken]);
    return Promise.resolve({
      groupPreview: {
        title: '',
        sharedTabs: [],
        statusCode: AbslStatusCode.kOk,
      },
    });
  }

  openTabGroup(groupId: string) {
    this.methodCalled('openTabGroup', groupId);
  }

  onGroupAction(action: GroupAction, progress: GroupActionProgress) {
    this.methodCalled('onGroupAction', [action, progress]);
  }
}

suite('Start flows', () => {
  let testDataSharingSdk: TestMock<DataSharingSdkImpl>&DataSharingSdkImpl;
  let dataSharingApp: DataSharingApp|null = null;
  let testPageHandler: TestDataSharingPageHandler;
  let callbackRouterRemote: PageRemote;

  setup(() => {
    testPageHandler = new TestDataSharingPageHandler();
    const {instance, remote} =
        browserProxyFactory.createForTest(testPageHandler);
    browserProxyFactory.setInstance(instance);
    callbackRouterRemote = remote;
    testDataSharingSdk = TestMock.fromClass(DataSharingSdkImpl);
    DataSharingSdkImpl.setInstance(testDataSharingSdk);
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
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runInviteFlow'));
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);
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
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();

    assertEquals(1, testPageHandler.getCallCount('makeTabGroupShared'));
    assertEquals(1, testPageHandler.getCallCount('getShareLink'));
    assertEquals(1, testDataSharingSdk.getCallCount('runInviteFlow'));
  });

  test('Manage flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=manage&group_id=fake_id&is_disabled_for_policy=true&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runManageFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runManageFlow')[0].groupId);
    assertEquals(
        true, testDataSharingSdk.getArgs('runManageFlow')[0].isSharingDisabled);
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);
  });

  test('Leave flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing/?flow=leave&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.LEAVE_GROUP, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runManageFlow'));
    const arg = testDataSharingSdk.getArgs('runManageFlow')[0];
    assertEquals('fake_id', arg.groupId);
    assertEquals(true, arg.showLeaveDialogAtStartup);
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);

    assertEquals(1, testPageHandler.getCallCount('onGroupAction'));
    assertDeepEquals(
        [GroupAction.kLeaveGroup, GroupActionProgress.kSuccess],
        testPageHandler.getArgs('onGroupAction')[0]);
  });

  test('Delete flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=delete&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.DELETE_GROUP, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runDeleteFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runDeleteFlow')[0].groupId);
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);

    assertEquals(1, testPageHandler.getCallCount('onGroupAction'));
    assertDeepEquals(
        [GroupAction.kDeleteGroup, GroupActionProgress.kSuccess],
        testPageHandler.getArgs('onGroupAction')[0]);
  });

  test('Close flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=close&group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runCloseFlow'));
    assertEquals(
        'fake_id', testDataSharingSdk.getArgs('runCloseFlow')[0].groupId);
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);
  });

  test('Join flow', async () => {
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.setSuccessfullyJoinedForTesting();
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('showUI'));
    assertEquals(1, testDataSharingSdk.getCallCount('runJoinFlow'));
    const arg = testDataSharingSdk.getArgs('runJoinFlow')[0];
    assertEquals('fake_group_id', arg.groupId);
    assertEquals('fake_token', arg.tokenSecret);
    // Do not close UI if successfully joined because it's supposed to wait
    // until the flow is complete.
    assertEquals(0, testPageHandler.getCallCount('closeUI'));
  });

  test('Join flow error case', async () => {
    // If join flows neither joined successfully nor abandon by user, we
    // consider it an error.
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.UNKNOWN, testPageHandler.getArgs('closeUI')[0]);
  });

  test('Join flow abandon case', async () => {
    // If user abandoned join by clicking on the cancel button, we still return
    // Code.OK when the dialog is closed.
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=join&group_id=fake_group_id&token_secret=fake_token');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
    document.body.appendChild(dataSharingApp);
    dataSharingApp.onEvent(
        {intentType: LoggingIntent.ABANDON_JOIN, progress: Progress.SUCCEEDED});
    await microtasksFinished();
    assertEquals(1, testPageHandler.getCallCount('closeUI'));
    assertEquals(Code.OK, testPageHandler.getArgs('closeUI')[0]);
  });

  test('Metrics reporting', async () => {
    loadTimeData.overrideValues({
      metricsReportingEnabled: true,
    });
    DataSharingApp.setUrlForTesting(
        'chrome-untrusted://data-sharing?flow=share&tab_group_id=fake_id');
    dataSharingApp = document.createElement('data-sharing-app');
    callbackRouterRemote.onAccessTokenFetched('fake_token');
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
