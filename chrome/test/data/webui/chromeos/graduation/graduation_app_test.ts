// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_app.js';

import {GraduationApp, Screens, ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('GraduationAppTest', function() {
  let graduationApp: GraduationApp;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationApp = new GraduationApp();
    document.body.appendChild(graduationApp);
    flush();
  });

  test('NavigateBetweenWelcomeAndTakeoutScreens', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_TAKEOUT_UI));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.TAKEOUT_UI);

    graduationApp.dispatchEvent(
        new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);
  });

  test('ShowErrorScreenPermanently', function() {
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.WELCOME);

    graduationApp.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_ERROR));
    assertEquals(graduationApp.getCurrentScreenForTest(), Screens.ERROR);

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
  });
});
