// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-manageemnt-permission-item. */
import 'chrome://app-settings/permission_item.js';

import type {PermissionItemElement} from 'chrome://app-settings/permission_item.js';
import {TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {PageHandlerRemote} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {MetricsBrowserProxy} from 'chrome://resources/cr_components/app_management/metrics_browser_proxy.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTestApp, setupMockHandler, TestMetricsBrowserProxy} from './app_management_test_support.js';

suite('AppManagementPermissionItemTest', function() {
  let permissionItem: PermissionItemElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let testMetricsProxy: TestMetricsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const app = createTestApp('app');
    handler = setupMockHandler();
    testMetricsProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxy.setInstance(testMetricsProxy);

    permissionItem = document.createElement('app-management-permission-item');
    permissionItem.app = app;
    permissionItem.permissionType = 'kLocation';
    document.body.appendChild(permissionItem);
    await microtasksFinished();
  });

  test('Toggle permission', async function() {
    assertFalse(getPermissionValueBool(
        permissionItem.app, permissionItem.permissionType));

    permissionItem.click();
    const data = await handler.whenCalled('setPermission');
    assertEquals(data[1].value.tristateValue, TriState.kAllow);

    const metricData =
        await testMetricsProxy.whenCalled('recordEnumerationValue');
    assertEquals(metricData[1], AppManagementUserAction.LOCATION_TURNED_ON);
  });
});
