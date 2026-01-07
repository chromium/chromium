// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {PresentationMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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

  test('presentation change', () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

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

    const state1 = chrome.readingMode.inSidePanelPresentationState;
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: state1}}));
    assertEquals(state1, presentationMenu.presentationState);

    const state2 = chrome.readingMode.inImmersiveOverlayPresentationState;
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: state2}}));
    assertEquals(state2, presentationMenu.presentationState);

    assertEquals(2, closeAllMenusCount);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    presentationMenu.open(document.body);
    assertTrue(presentationMenu.$.menu.$.lazyMenu.get().open);
    presentationMenu.close();
    assertFalse(presentationMenu.$.menu.$.lazyMenu.get().open);
  });
});
