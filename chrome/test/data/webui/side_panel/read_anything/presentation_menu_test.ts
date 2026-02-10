// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {PresentationMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertHeadersForDropdown, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('PresentationMenuElement', () => {
  let presentationMenu: PresentationMenuElement;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    presentationMenu = document.createElement('presentation-menu');
    presentationMenu.presentationState =
        chrome.readingMode.inImmersiveOverlayPresentationState;
    document.body.appendChild(presentationMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(presentationMenu);
  });

  test('does not have headers', () => {
    assertHeadersForDropdown(
        presentationMenu.$.menu, /*shouldHaveHeaders=*/ false);
  });

  test('presentation change', async () => {
    const sidePanelState = chrome.readingMode.inSidePanelPresentationState;
    const immersiveState =
        chrome.readingMode.inImmersiveOverlayPresentationState;
    chrome.readingMode.togglePresentation = () => {
      if (presentationMenu.presentationState === sidePanelState) {
        presentationMenu.presentationState = immersiveState;
      } else if (presentationMenu.presentationState === immersiveState) {
        presentationMenu.presentationState = sidePanelState;
      }
    };

    const closeAllMenusPromise1 =
    eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: sidePanelState}}));
    await closeAllMenusPromise1;
    assertEquals(sidePanelState, presentationMenu.presentationState);

    const closeAllMenusPromise2 =
    eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: immersiveState}}));
    await closeAllMenusPromise2;
    assertEquals(immersiveState, presentationMenu.presentationState);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    presentationMenu.open(document.body);
    assertTrue(presentationMenu.$.menu.$.lazyMenu.get().open);
    presentationMenu.close();
    assertFalse(presentationMenu.$.menu.$.lazyMenu.get().open);
  });
});
