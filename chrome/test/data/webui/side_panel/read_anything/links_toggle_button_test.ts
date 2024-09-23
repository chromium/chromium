
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LinksToggle', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement;
  let linksToggled: boolean;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    linksToggled = false;
    document.addEventListener(ToolbarEvent.LINKS, () => linksToggled = true);
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();
    menuButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
        '#' + LINK_TOGGLE_BUTTON_ID)!;
  });

  test('by default links are on and button is enabled', () => {
    assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
    assertTrue(chrome.readingMode.linksEnabled);
    assertStringContains('disable links', menuButton.title.toLowerCase());
    assertFalse(menuButton.disabled);
  });

  suite('on first click', () => {
    setup(() => {
      menuButton.click();
    });

    test('links are turned off', () => {
      assertEquals(LINKS_DISABLED_ICON, menuButton.ironIcon);
      assertFalse(chrome.readingMode.linksEnabled);
      assertStringContains('enable links', menuButton.title.toLowerCase());
    });

    test('event is propagated', () => {
      assertTrue(linksToggled);
    });

    test('when unpaused, button is disabled', () => {
      toolbar.isSpeechActive = true;
      assertTrue(menuButton.disabled);
    });

    test('when paused, button is enabled', () => {
      toolbar.isSpeechActive = false;
      assertFalse(menuButton.disabled);
    });

    suite('on next click', () => {
      setup(() => {
        menuButton.click();
      });

      test('links are turned back on', () => {
        assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
        assertTrue(chrome.readingMode.linksEnabled);
        assertStringContains('disable links', menuButton.title.toLowerCase());
      });

      test('event is propagated', () => {
        assertTrue(linksToggled);
      });

      test('when unpaused, button is disabled', () => {
        toolbar.isSpeechActive = true;
        assertTrue(menuButton.disabled);
      });

      test('when paused, button is enabled', () => {
        toolbar.isSpeechActive = false;
        assertFalse(menuButton.disabled);
      });
    });
  });
});
