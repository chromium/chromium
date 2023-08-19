// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {addApp, AppManagementPageState, changeApp, reduceAction, removeApp, updateApps, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createInitialState} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createApp} from '../../app_management/test_util.js';

suite('app state', () => {
  let apps: Record<string, App>;

  setup(() => {
    apps = {
      '1': createApp('1', {}),
      '2': createApp('2', {}),
    };
  });

  test('updates when an app is added', () => {
    const newApp = createApp('3', {type: 1, title: 'a'});
    const action = addApp(newApp);
    apps = updateApps(apps, action);

    // Check that apps contains a key for each app id.
    assertTrue(!!apps['1']);
    assertTrue(!!apps['2']);
    assertTrue(!!apps['3']);

    // Check that id corresponds to the right app.
    const app = apps['3'];
    assertEquals('3', app.id);
    assertEquals(1, app.type);
    assertEquals('a', app.title);
  });

  test('updates when an app is changed', () => {
    const changedApp = createApp('2', {type: 1, title: 'a'});
    const action = changeApp(changedApp);
    apps = updateApps(apps, action);

    // Check that app has changed.
    const app = apps['2'];
    assertTrue(!!app);
    assertEquals(1, app.type);
    assertEquals('a', app.title);

    // Check that number of apps hasn't changed.
    assertEquals(2, Object.keys(apps).length);
  });

  test('updates when an app is removed', () => {
    const action = removeApp('1');
    apps = updateApps(apps, action);

    // Check that app is removed.
    assertEquals(undefined, apps['1']);

    // Check that other app is unaffected.
    assertTrue(!!apps['2']);
  });
});

suite('selected app id', () => {
  let state: AppManagementPageState;

  setup(() => {
    state = createInitialState(
        [
          createApp('1', {}),
          createApp('2', {}),
        ],
        {});
  });

  test('initial state has no selected app', () => {
    assertNull(state.selectedAppId);
  });

  test('updates selected app id', () => {
    let action = updateSelectedAppId('1');
    state = reduceAction(state, action);
    assertEquals('1', state.selectedAppId);

    action = updateSelectedAppId('2');
    state = reduceAction(state, action);
    assertEquals('2', state.selectedAppId);

    action = updateSelectedAppId(null);
    state = reduceAction(state, action);
    assertNull(state.selectedAppId);
  });

  test('removing an app resets selected app id', () => {
    const selectAction = updateSelectedAppId('1');
    state = reduceAction(state, selectAction);
    assertEquals('1', state.selectedAppId);

    const removeAction = removeApp('1');
    state = reduceAction(state, removeAction);
    assertNull(state.selectedAppId);
  });
});
