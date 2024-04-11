// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LETTER_SPACING_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {getItemsInMenu, stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LetterSpacing', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let spacingEmitted: number;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    spacingEmitted = -1;
    document.addEventListener(LETTER_SPACING_EVENT, event => {
      spacingEmitted = (event as CustomEvent).detail.data;
    });
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();
  });

  test('is dropdown menu', () => {
    stubAnimationFrame();
    const menuButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
        '#letter-spacing');

    menuButton!.click();
    flush();

    assertTrue(toolbar.$.letterSpacingMenu.get().open);
  });

  suite('menu', () => {
    let letterSpacingMenuOptions: HTMLButtonElement[];

    setup(() => {
      letterSpacingMenuOptions = getItemsInMenu(toolbar.$.letterSpacingMenu);
    });

    test('has 3 options', () => {
      assertEquals(letterSpacingMenuOptions.length, 3);
    });

    test('first option propagates standard spacing', () => {
      letterSpacingMenuOptions[0]!.click();

      assertEquals(spacingEmitted, chrome.readingMode.standardLetterSpacing);
      assertEquals(
          chrome.readingMode.letterSpacing,
          chrome.readingMode.standardLetterSpacing);
    });

    test('second option propagates wide spacing', () => {
      letterSpacingMenuOptions[1]!.click();

      assertEquals(spacingEmitted, chrome.readingMode.wideLetterSpacing);
      assertEquals(
          chrome.readingMode.letterSpacing,
          chrome.readingMode.wideLetterSpacing);
    });

    test('third option propagates very wide spacing', () => {
      letterSpacingMenuOptions[2]!.click();

      assertEquals(spacingEmitted, chrome.readingMode.veryWideLetterSpacing);
      assertEquals(
          chrome.readingMode.letterSpacing,
          chrome.readingMode.veryWideLetterSpacing);
    });
  });
});
