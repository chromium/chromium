// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {THEME_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {getItemsInMenu, stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('ColorMenu', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let colorEmitted: string;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    colorEmitted = '';
    document.addEventListener(THEME_EVENT, event => {
      colorEmitted = (event as CustomEvent).detail.data;
    });
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();
  });

  test('is dropdown menu', () => {
    stubAnimationFrame();
    const menuButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#color');

    menuButton!.click();
    flush();

    assertTrue(toolbar.$.colorMenu.get().open);
  });

  suite('menu', () => {
    let colorMenuOptions: HTMLButtonElement[];

    setup(() => {
      colorMenuOptions = getItemsInMenu(toolbar.$.colorMenu);
    });

    test('option click propagates change', () => {
      const emittedColors: string[] = [];
      let previousPropagatedColor = -1;
      colorMenuOptions.forEach(option => {
        option.click();

        // the selected option is unique and is emitted down the pipeline
        assertEquals(emittedColors.indexOf(colorEmitted), -1);
        assertGT(chrome.readingMode.colorTheme, previousPropagatedColor);

        emittedColors.push(colorEmitted);
        previousPropagatedColor = chrome.readingMode.colorTheme;
      });
    });
  });
});
