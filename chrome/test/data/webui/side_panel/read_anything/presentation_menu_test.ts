// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {PresentationMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';
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

  test('restores saved presentation state', async () => {
    const sidePanelState = visualBrowserProxy.getInSidePanelPresentationState();
    assertNotEquals(sidePanelState, presentationMenu.$.menu.selectedValue);

    presentationMenu.presentationState = sidePanelState;
    await microtasksFinished();

    assertEquals(sidePanelState, presentationMenu.$.menu.selectedValue);
  });

  test('presentation change closes all menus', async () => {
    const sidePanelState = visualBrowserProxy.getInSidePanelPresentationState();
    const closeAllMenusPromise =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    presentationMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: sidePanelState}}));
    await closeAllMenusPromise;

    assertEquals(0, visualBrowserProxy.getCallCount('togglePresentation'));
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    presentationMenu.open(document.body);
    assertTrue(presentationMenu.$.menu.$.lazyMenu.get().open);
    presentationMenu.close();
    assertFalse(presentationMenu.$.menu.$.lazyMenu.get().open);
  });
});
