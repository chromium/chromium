// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_app.js';

import {GraduationApp, Screens, ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {resetGraduationHandlerForTesting, setGraduationUiHandlerForTesting} from 'chrome://graduation/js/graduation_ui_handler.js';
import {AuthResult, GraduationScreen} from 'chrome://graduation/mojom/graduation_ui.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestGraduationUiHandler} from './test_graduation_ui_handler.js';

suite('GraduationAppTest.AuthenticationSuccess', function() {
  let handler: TestGraduationUiHandler;
  let graduationApp: GraduationApp;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = new TestGraduationUiHandler();
    handler.setAuthResult(AuthResult.kSuccess);
    setGraduationUiHandlerForTesting(handler);

    graduationApp = new GraduationApp();

    // Set an empty webview URL to avoid a loadabort event in the Takeout
    // webview.
    loadTimeData.overrideValues({startTransferUrl: ''});

    document.body.appendChild(graduationApp);

    await flushTasks();
  });

  teardown(async () => {
    resetGraduationHandlerForTesting();
  });

  test('Navigate between Welcome and Takeout screens', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);
    assertEquals(handler.getCallCount('authenticateWebview'), 1);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_TAKEOUT_UI));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.TAKEOUT_UI);
    assertEquals(handler.getLastScreen(), GraduationScreen.kTakeoutUi);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);
  });

  test('Error screen is terminal', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getCallCount('authenticateWebview'), 1);

    graduationApp.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_ERROR));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);
    assertEquals(handler.getLastScreen(), GraduationScreen.kError);

    // Error screen should permanently show even if other screens are triggered.
    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_TAKEOUT_UI));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);

    window.dispatchEvent(new Event(ScreenSwitchEvents.OFFLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);

    window.dispatchEvent(new Event(ScreenSwitchEvents.ONLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);
  });

  test('Error screen is not shown when app is offline', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getCallCount('authenticateWebview'), 1);

    window.dispatchEvent(new Event(ScreenSwitchEvents.OFFLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.OFFLINE);

    graduationApp.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_ERROR));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.OFFLINE);
  });

  test('Offline screen is shown until app is online', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getCallCount('authenticateWebview'), 1);

    window.dispatchEvent(new Event(ScreenSwitchEvents.OFFLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.OFFLINE);

    window.dispatchEvent(new Event(ScreenSwitchEvents.ONLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);
  });
});

suite('GraduationAppTest.AuthenticationError', function() {
  let handler: TestGraduationUiHandler;
  let graduationApp: GraduationApp;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = new TestGraduationUiHandler();
    handler.setAuthResult(AuthResult.kError);
    setGraduationUiHandlerForTesting(handler);

    graduationApp = new GraduationApp();

    // Set an empty webview URL to avoid a loadabort event in the Takeout
    // webview.
    loadTimeData.overrideValues({startTransferUrl: ''});

    document.body.appendChild(graduationApp);

    await flushTasks();
  });

  teardown(async () => {
    resetGraduationHandlerForTesting();
  });

  test('Error screen is shown when authentication has failed', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);
    assertEquals(handler.getLastScreen(), GraduationScreen.kError);
    assertEquals(handler.getCallCount('authenticateWebview'), 1);
  });
});
