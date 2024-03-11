
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, LINKS_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LinksToggle', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement;
  let linksToggled: boolean;

  function toolbarPaused(paused: boolean) {
    // Bypass Typescript compiler to allow us to get a private property
    // @ts-ignore
    toolbar.paused = paused;
  }

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    linksToggled = false;
    document.addEventListener(LINKS_EVENT, () => linksToggled = true);
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    menuButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
        '#' + LINK_TOGGLE_BUTTON_ID)!;
  });

  suite('by default', () => {
    test('links are on', () => {
      assertEquals(menuButton.ironIcon, LINKS_ENABLED_ICON);
      assertEquals(chrome.readingMode.linksEnabled, true);
      assertStringContains(menuButton.title.toLowerCase(), 'disable');
    });

    test('button is enabled', () => {
      assertFalse(menuButton.disabled);
    });
  });

  suite('on first click', () => {
    setup(() => {
      menuButton.click();
    });

    test('links are turned off', () => {
      assertEquals(menuButton.ironIcon, LINKS_DISABLED_ICON);
      assertEquals(chrome.readingMode.linksEnabled, false);
      assertStringContains(menuButton.title.toLowerCase(), 'enable');
    });

    test('event is propagated', () => {
      assertTrue(linksToggled);
    });

    test('when unpaused, button is disabled', () => {
      toolbarPaused(false);
      assertTrue(menuButton.disabled);
    });

    test('when paused, button is enabled', () => {
      toolbarPaused(true);
      assertFalse(menuButton.disabled);
    });

    suite('on next click', () => {
      setup(() => {
        menuButton.click();
      });

      test('links are turned back on', () => {
        assertEquals(menuButton.ironIcon, LINKS_ENABLED_ICON);
        assertEquals(chrome.readingMode.linksEnabled, true);
        assertStringContains(menuButton.title.toLowerCase(), 'disable');
      });

      test('event is propagated', () => {
        assertTrue(linksToggled);
      });

      test('when unpaused, button is disabled', () => {
        toolbarPaused(false);
        assertTrue(menuButton.disabled);
      });

      test('when paused, button is enabled', () => {
        toolbarPaused(true);
        assertFalse(menuButton.disabled);
      });
    });
  });
});
