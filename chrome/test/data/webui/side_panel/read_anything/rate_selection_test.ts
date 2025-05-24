// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ReadAloudSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {getItemsInMenu, mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('RateSelection', () => {
  let toolbar: ReadAnythingToolbarElement;
  let rateButton: CrIconButtonElement;
  let rateEmitted: boolean;
  let metrics: TestMetricsBrowserProxy;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    metrics = mockMetrics();

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);

    await microtasksFinished();
    rateButton =
        toolbar.shadowRoot.querySelector<CrIconButtonElement>('#rate')!;
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

  test('menu button opens menu', async () => {
    stubAnimationFrame();

    rateButton.click();
    await microtasksFinished();

    assertTrue(toolbar.$.rateMenu.get().open);
  });

  suite('dropdown menu', () => {
    let options: HTMLButtonElement[];

    setup(() => {
      options = getItemsInMenu(toolbar.$.rateMenu);
    });

    test(
        'displays options in increasing order with multiple options',
        async () => {
          assertGT(options.length, 0);

          let previousRate = -1;

          for (const option of options) {
            option.click();
            await microtasksFinished();
            const newRate = chrome.readingMode.speechRate;
            assertGT(newRate, previousRate);
            assertTrue(rateEmitted);
            previousRate = newRate;
            rateEmitted = false;
          }
        });

    test('on option click', async () => {
      assertTrue(toolbar.rateOptions.length >= 1);
      const rateValue = toolbar.rateOptions[0];
      const menuOption = options[0]!;
      menuOption.click();
      await microtasksFinished();

      // updates rate
      assertEquals(rateValue, chrome.readingMode.speechRate);
      assertTrue(rateEmitted);

      // updates icon on toolbar
      assertEquals('voice-rate:' + rateValue, rateButton.ironIcon);
      assertEquals(
          ReadAloudSettingsChange.VOICE_SPEED_CHANGE,
          await metrics.whenCalled('recordVoiceSpeed'));

      // closes menu
      assertFalse(toolbar.$.rateMenu.get().open);
    });
  });
});
