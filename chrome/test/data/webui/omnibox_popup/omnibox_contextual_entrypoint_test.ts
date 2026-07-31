// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxContextualEntrypointButtonElement, OmniboxPopupPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDefaultInputState} from './test_searchbox_browser_proxy.js';

suite('OmniboxContextualEntrypointTest', () => {
  let element: OmniboxContextualEntrypointButtonElement;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxAimPopupEnabled: true,
      searchboxLayoutMode: 'TallBottomContext',
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      omniboxShowContextButtonSuggestionLabel: false,
    });

    handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance, remote} =
        omniboxPopupBrowserProxyFactory.createForTest(handler);
    callbackRouter = remote;
    omniboxPopupBrowserProxyFactory.setInstance(instance);

    element = document.createElement('omnibox-contextual-entrypoint-button');
    element.inputState = {
      ...createDefaultInputState(),
      allowedInputTypes: [0],
    };
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('ContextMenuEntrypointClickSingleMojoCall', async () => {
    const innerButton = element.shadowRoot?.querySelector<HTMLElement>(
        'cr-composebox-contextual-entrypoint-button');
    assertTrue(!!innerButton);

    innerButton.dispatchEvent(new CustomEvent('context-menu-entrypoint-click', {
      detail: {x: 100, y: 200},
      bubbles: true,
      composed: true,
    }));

    assertEquals(1, handler.getCallCount('showContextMenu'));
    const point = await handler.whenCalled('showContextMenu');
    assertDeepEquals({x: 100, y: 200}, point);
    assertTrue(element.classList.contains('menu-open'));
  });

  test('OnShowBlurBehavior', async () => {
    element.focus();
    await microtasksFinished();

    element.showContextMenu({x: 1, y: 1});
    await microtasksFinished();
    assertTrue(element.classList.contains('menu-open'));

    callbackRouter.onShow();
    await microtasksFinished();

    assertFalse(element.matches(':focus-within'));
    assertFalse(element.classList.contains('menu-open'));
  });

  test('RapidHasPopupFocusToggling', async () => {
    element.hasPopupFocus = true;
    element.hasPopupFocus = false;
    element.hasPopupFocus = true;
    element.hasPopupFocus = false;
    element.hasPopupFocus = true;
    await microtasksFinished();

    assertTrue(element.hasPopupFocus);
    const innerEntrypoint =
        element.shadowRoot
            ?.querySelector<HTMLElement&{hasPopupFocus?: boolean}>(
                'cr-composebox-contextual-entrypoint-button');
    assertTrue(!!innerEntrypoint);
    assertTrue(
        innerEntrypoint.hasPopupFocus ??
        innerEntrypoint.hasAttribute('has-popup-focus'));
  });

  test('RapidShowContextMenuCycles', async () => {
    for (let i = 0; i < 5; i++) {
      element.showContextMenu({x: i, y: i});
    }
    await microtasksFinished();
    assertTrue(element.classList.contains('menu-open'));

    callbackRouter.onContextMenuClosed();
    await microtasksFinished();
    assertFalse(element.classList.contains('menu-open'));

    element.showContextMenu({x: 10, y: 10});
    await microtasksFinished();
    assertTrue(element.classList.contains('menu-open'));

    callbackRouter.onContextMenuClosed();
    await microtasksFinished();
    assertFalse(element.classList.contains('menu-open'));
  });

  test('SpuriousOnContextMenuClosedCalls', async () => {
    callbackRouter.onContextMenuClosed();
    callbackRouter.onContextMenuClosed();
    callbackRouter.onContextMenuClosed();
    await microtasksFinished();

    assertFalse(element.classList.contains('menu-open'));
  });

  test('DisconnectWhileContextMenuOpenLeaksState', async () => {
    element.showContextMenu({x: 5, y: 5});
    await microtasksFinished();
    assertTrue(element.classList.contains('menu-open'));

    element.remove();
    await microtasksFinished();

    callbackRouter.onContextMenuClosed();
    await microtasksFinished();

    document.body.appendChild(element);
    await microtasksFinished();

    assertFalse(element.classList.contains('menu-open'));
  });
});
