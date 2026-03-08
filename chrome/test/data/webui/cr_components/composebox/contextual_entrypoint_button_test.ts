// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createValidInputState} from './composebox_test_utils.js';

suite('ContextualEntrypointButton', () => {
  let entrypointButton: ContextualEntrypointButtonElement;

  function createEntrypointButton(): ContextualEntrypointButtonElement {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element =
        document.createElement('cr-composebox-contextual-entrypoint-button');
    element.inputState = createValidInputState();
    document.body.appendChild(element);
    return element;
  }

  setup(async () => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: true,
    });

    entrypointButton = createEntrypointButton();
    await microtasksFinished();
  });

  test('clicking entrypoint fires event', async () => {
    // Act.
    const eventPromise =
        eventToPromise('context-menu-entrypoint-click', entrypointButton);
    const entrypoint = $$(entrypointButton, '#entrypoint');
    assertTrue(!!entrypoint);
    entrypoint.click();
    await microtasksFinished();

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('invalid input state disables entrypoint', async () => {
    entrypointButton.inputState = null;
    await microtasksFinished();

    const entrypoint = $$(entrypointButton, '#entrypoint');
    assertFalse(!!entrypoint);
  });

  test('hides description when window is narrow', async () => {
    class MockMediaQueryList extends EventTarget implements MediaQueryList {
      matches: boolean = false;
      media: string = '(width <= 264px)';
      onchange = null;

      addListener(
          callback: ((this: MediaQueryList, ev: MediaQueryListEvent) => any)|
          null): void {
        this.addEventListener('change', callback as EventListener);
      }

      removeListener(
          callback: ((this: MediaQueryList, ev: MediaQueryListEvent) => any)|
          null): void {
        this.removeEventListener('change', callback as EventListener);
      }

      // Custom helper to simulate the browser window resizing.
      simulateResize(matches: boolean) {
        this.matches = matches;
        const event = new Event('change') as Event & {matches: boolean};
        event.matches = matches;
        this.dispatchEvent(event);
      }
    }

    const mockMql = new MockMediaQueryList();

    const windowProxy = TestMock.fromClass(WindowProxy);
    windowProxy.setResultMapperFor('matchMedia', (query: string) => {
      if (query === '(width <= 264px)') {
        return mockMql;
      }
      return window.matchMedia(query);
    });

    WindowProxy.setInstance(windowProxy);

    const testElement = createEntrypointButton();
    testElement.showContextMenuDescription = true;
    await microtasksFinished();

    // Assert initial wide state.
    let description = $$(testElement, '#description');
    assertTrue(!!description);

    // Act: Simulate the window becoming narrow by firing the change event.
    mockMql.simulateResize(true);
    await microtasksFinished();

    // Assert narrow state.
    description = $$(testElement, '#description');
    assertFalse(!!description);
  });
});
