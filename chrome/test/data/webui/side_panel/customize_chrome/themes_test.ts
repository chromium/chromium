// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/themes.js';

import {ThemesElement} from 'chrome://customize-chrome-side-panel.top-chrome/themes.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ThemesTest', () => {
  let themesElement: ThemesElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    themesElement = document.createElement('customize-chrome-themes');
    document.body.appendChild(themesElement);
  });

  test('themes buttons create events', async () => {
    // Check that clicking the back button produces a back-click event.
    let eventPromise = eventToPromise('back-click', themesElement);
    themesElement.$.backButton.click();
    let event = await eventPromise;
    assertTrue(!!event);

    // Check that clicking a theme produces a theme-select event.
    eventPromise = eventToPromise('theme-select', themesElement);
    const theme =
        themesElement.shadowRoot!.querySelector('.theme')! as HTMLButtonElement;
    theme.click();
    event = await eventPromise;
    assertTrue(!!event);
  });
});
