// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {getItemsInMenu, stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('RateSelection', () => {
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let rateButton: CrIconButtonElement;
  let rateEmitted: boolean;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();
    rateButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#rate')!;
    rateEmitted = false;
    document.addEventListener(ToolbarEvent.RATE, () => {
      rateEmitted = true;
    });
  });

  test('by default', () => {
    // Uses 1x
    assertEquals('voice-rate:1', rateButton.ironIcon);
    assertEquals(1, chrome.readingMode.speechRate);

    // Menu is not open
    assertFalse(toolbar.$.rateMenu.get().open);
  });

  test('menu button opens menu', () => {
    stubAnimationFrame();

    rateButton.click();
    flush();

    assertTrue(toolbar.$.rateMenu.get().open);
  });

  suite('dropdown menu', () => {
    let options: HTMLButtonElement[];

    setup(() => {
      options = getItemsInMenu(toolbar.$.rateMenu);
    });

    test('displays options in increasing order with multiple options', () => {
      assertGT(options.length, 0);

      let previousRate = -1;
      options.forEach((option) => {
        option.click();
        const newRate = chrome.readingMode.speechRate;
        assertGT(newRate, previousRate);
        assertTrue(rateEmitted);
        previousRate = newRate;
        rateEmitted = false;
      });
    });

    test('on option click', () => {
      assertTrue(toolbar.rateOptions.length >= 1);
      const rateValue = toolbar.rateOptions[0];
      const menuOption = options[0]!;
      menuOption.click();

      // updates rate
      assertEquals(rateValue, chrome.readingMode.speechRate);
      assertTrue(rateEmitted);

      // updates icon on toolbar
      assertEquals('voice-rate:' + rateValue, rateButton.ironIcon);

      // closes menu
      assertFalse(toolbar.$.rateMenu.get().open);
    });
  });
});
