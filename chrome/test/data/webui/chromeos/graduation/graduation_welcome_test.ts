// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_welcome.js';
import 'chrome://graduation/strings.m.js';

import {ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {GraduationWelcome} from 'chrome://graduation/js/graduation_welcome.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('GraduationWelcomeTest', function() {
  let welcomeScreen: GraduationWelcome;

  function getActionButton(): CrButtonElement {
    const getStartedButton =
        welcomeScreen.shadowRoot!.querySelector<CrButtonElement>(
            '#getStartedButton');
    assertTrue(!!getStartedButton);
    return getStartedButton;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    welcomeScreen = new GraduationWelcome();
    document.body.appendChild(welcomeScreen);
    flush();
  });

  teardown(() => {
    welcomeScreen.remove();
  });

  test('TriggerTakeoutPageOnGetStartedButtonClick', function() {
    let takeoutPageTriggered = false;
    welcomeScreen.addEventListener(ScreenSwitchEvents.SHOW_TAKEOUT_UI, () => {
      takeoutPageTriggered = true;
    });
    getActionButton().click();
    assertTrue(takeoutPageTriggered);
  });
});
