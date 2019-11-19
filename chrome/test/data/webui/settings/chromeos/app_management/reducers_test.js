// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('app state', function() {
  let apps;

  setup(function() {
    apps = {
      '1': createApp('1'),
      '2': createApp('2'),
    };
  });

  test('updates when an app is added', function() {
    const newApp = createApp('3', {type: 1, title: 'a'});
    const action = app_management.actions.addApp(newApp);
    apps = app_management.AppState.updateApps(apps, action);

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

  test('updates when an app is changed', function() {
    const changedApp = createApp('2', {type: 1, title: 'a'});
    const action = app_management.actions.changeApp(changedApp);
    apps = app_management.AppState.updateApps(apps, action);

    // Check that app has changed.
    const app = apps['2'];
    assertEquals(1, app.type);
    assertEquals('a', app.title);

    // Check that number of apps hasn't changed.
    assertEquals(Object.keys(apps).length, 2);
  });

  test('updates when an app is removed', function() {
    const action = app_management.actions.removeApp('1');
    apps = app_management.AppState.updateApps(apps, action);

    // Check that app is removed.
    assertFalse(!!apps['1']);

    // Check that other app is unaffected.
    assertTrue(!!apps['2']);
  });
});

suite('selected app id', function() {
  let state;

  setup(function() {
    state = app_management.util.createInitialState([
      createApp('1'),
      createApp('2'),
    ]);
  });

  test('initial state has no selected app', function() {
    assertEquals(null, state.selectedAppId);
  });

  test('updates selected app id', function() {
    let action = app_management.actions.updateSelectedAppId('1');
    state = app_management.reduceAction(state, action);
    assertEquals('1', state.selectedAppId);

    action = app_management.actions.updateSelectedAppId('2');
    state = app_management.reduceAction(state, action);
    assertEquals('2', state.selectedAppId);

    action = app_management.actions.updateSelectedAppId(null);
    state = app_management.reduceAction(state, action);
    assertEquals(null, state.selectedAppId);
  });

  test('removing an app resets selected app id', function() {
    let action = app_management.actions.updateSelectedAppId('1');
    state = app_management.reduceAction(state, action);
    assertEquals('1', state.selectedAppId);

    action = app_management.actions.removeApp('1');
    state = app_management.reduceAction(state, action);
    assertEquals(null, state.selectedAppId);
  });
});
