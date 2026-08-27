// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {PresentationMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('PresentationMenuElement', () => {
  let presentationMenu: PresentationMenuElement;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

    presentationMenu = document.createElement('presentation-menu');
    presentationMenu.presentationState =
        visualBrowserProxy.getInImmersiveOverlayPresentationState();
    document.body.appendChild(presentationMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(presentationMenu);
  });

  test('presentation change', async () => {
    const sidePanelState = visualBrowserProxy.getInSidePanelPresentationState();
    const immersiveState =
        visualBrowserProxy.getInImmersiveOverlayPresentationState();

    const closeAllMenusPromise1 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: sidePanelState}}));
    await closeAllMenusPromise1;
    assertEquals(1, visualBrowserProxy.getCallCount('togglePresentation'));

    presentationMenu.presentationState = sidePanelState;

    const closeAllMenusPromise2 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: immersiveState}}));
    await closeAllMenusPromise2;
    assertEquals(2, visualBrowserProxy.getCallCount('togglePresentation'));
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    presentationMenu.open(document.body);
    assertTrue(presentationMenu.$.menu.$.lazyMenu.get().open);
    presentationMenu.close();
    assertFalse(presentationMenu.$.menu.$.lazyMenu.get().open);
  });
});
