// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_app.js';

import {GraduationApp, Screens, ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {resetGraduationHandlerForTesting, setGraduationUiHandlerForTesting} from 'chrome://graduation/js/graduation_ui_handler.js';
import {GraduationScreen} from 'chrome://graduation/mojom/graduation_ui.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestGraduationUiHandler} from './test_graduation_ui_handler.js';

suite('GraduationAppTest', function() {
  let handler: TestGraduationUiHandler;
  let graduationApp: GraduationApp;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = new TestGraduationUiHandler();
    setGraduationUiHandlerForTesting(handler);

    graduationApp = new GraduationApp();

    // Set a mock webview URL to avoid a loadabort event in the Takeout webview.
    loadTimeData.overrideValues({webviewUrl: ''});

    document.body.appendChild(graduationApp);

    await flushTasks();
  });

  teardown(async () => {
    resetGraduationHandlerForTesting();
  });

  test('NavigateBetweenWelcomeAndTakeoutScreens', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_TAKEOUT_UI));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.TAKEOUT_UI);
    assertEquals(handler.getLastScreen(), GraduationScreen.kTakeoutUi);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);
  });

  test('ShowErrorScreenPermanently', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);

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

  test('ShowOfflineScreenUntilBackOnline', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);

    window.dispatchEvent(new Event(ScreenSwitchEvents.OFFLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.OFFLINE);

    window.dispatchEvent(new Event(ScreenSwitchEvents.ONLINE));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
    assertEquals(handler.getLastScreen(), GraduationScreen.kWelcome);
  });
});
