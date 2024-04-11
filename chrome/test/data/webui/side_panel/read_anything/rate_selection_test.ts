// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RATE_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {getItemsInMenu, stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('RateSelection', () => {
  let toolbar: ReadAnythingToolbarElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let rateButton: CrIconButtonElement;
  let rateEmitted: number;

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
    rateEmitted = -1;
    document.addEventListener(RATE_EVENT, event => {
      rateEmitted = (event as CustomEvent).detail.rate;
    });
  });

  suite('by default', () => {
    test('uses 1x', () => {
      assertEquals(rateButton.ironIcon, 'voice-rate:1');
      assertEquals(chrome.readingMode.speechRate, 1);
    });

    test('menu is not open', () => {
      assertFalse(toolbar.$.rateMenu.get().open);
    });
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

    test('has multiple options', () => {
      assertGT(options.length, 0);
    });

    test('displays options in increasing order', () => {
      let previousRate = -1;
      options.forEach((option) => {
        option.click();
        const newRate = rateEmitted;
        assertGT(newRate, previousRate);
        previousRate = newRate;
      });
    });

    suite('on option click', () => {
      let menuOption: HTMLButtonElement;
      let rateValue: number;

      setup(() => {
        // Bypass Typescript compiler to allow us to get a private property
        // @ts-ignore
        rateValue = toolbar.rateOptions_[0];
        menuOption = options[0]!;
        menuOption.click();
      });

      test('updates rate', () => {
        assertEquals(chrome.readingMode.speechRate, rateValue);
        assertEquals(rateEmitted, rateValue);
      });

      test('updates icon on toolbar', () => {
        assertEquals(rateButton.ironIcon, 'voice-rate:' + rateValue);
      });

      test('closes menu', () => {
        assertFalse(toolbar.$.rateMenu.get().open);
      });
    });
  });
});
