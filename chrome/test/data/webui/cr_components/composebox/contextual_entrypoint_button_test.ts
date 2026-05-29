// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextualEntrypointButton', () => {
  let entrypointButton: ContextualEntrypointButtonElement;

  function createEntrypointButton(): ContextualEntrypointButtonElement {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element =
        document.createElement('cr-composebox-contextual-entrypoint-button');
    document.body.appendChild(element);
    return element;
  }

  setup(async () => {
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

  test('ShowContextMenuDescription', async () => {
    loadTimeData.overrideValues({
      addContext: 'Add tabs and more',
    });

    const testElement = createEntrypointButton();
    testElement.showContextMenuDescription = false;
    await microtasksFinished();

    assertFalse(!!$$(testElement, '#description'));

    testElement.showContextMenuDescription = true;
    await microtasksFinished();

    const description = $$(testElement, '#description');
    assertTrue(!!description);
    assertEquals('Add tabs and more', description.textContent.trim());
  });

  test('disables animations when --cr-animations-disabled is 1', async () => {
    document.body.style.setProperty('--cr-animations-disabled', '1');
    const testElement = createEntrypointButton();
    testElement.showContextMenuDescription = true;
    testElement.setAttribute('glif-animation-state', 'started');
    await microtasksFinished();

    // selector: The query selector for the element to check.
    // style_assertion: The expected styles for the element.
    const checks = [
      {
        selector: '.aim-gradient-outer-blur',
        style_assertion: {
          'display': 'none',
        },
      },
      {
        selector: '.aim-gradient-solid',
        style_assertion: {
          'display': 'none',
        },
      },
      {
        selector: '.aim-background',
        style_assertion: {
          'display': 'none',
        },
      },
      {
        selector: '#entrypoint',
        style_assertion: {
          'animation-name': 'none',
        },
      },
      {
        selector: '#description',
        style_assertion: {
          'animation-name': 'none',
          'opacity': '1',
          'transform': 'none',
        },
      },
    ];

    for (const check of checks) {
      const element = $$(testElement, check.selector);
      assertTrue(!!element, `Expect an element with ${check.selector}`);
      const style = window.getComputedStyle(element);
      const actualStyles: Record<string, string> = {};
      for (const property of Object.keys(check.style_assertion)) {
        actualStyles[property] =
            style[property as keyof CSSStyleDeclaration] as unknown as string;
      }
      assertDeepEquals(
          actualStyles, check.style_assertion,
          `Expect ${check.selector} to have the following styles: ${
              JSON.stringify(check.style_assertion)}, got: ${
              JSON.stringify(actualStyles)}`);
    }

    // Clean up
    document.body.style.removeProperty('--cr-animations-disabled');
  });

  test('onIconAnimationend transitions to finished state', async () => {
    const testElement = createEntrypointButton();
    testElement.showContextMenuDescription = true;
    testElement.energyEffectAnimationEnabled = true;
    testElement.setAttribute('glif-animation-state', 'started');
    await microtasksFinished();

    assertEquals('started', testElement.getAttribute('glif-animation-state'));

    // Act: Simulate the icon-rotate animation completing.
    const icon = $$(testElement, '#entrypointIcon');
    assertTrue(!!icon);
    icon.dispatchEvent(
        new AnimationEvent('animationend', {animationName: 'icon-rotate'}));
    await microtasksFinished();

    // Assert.
    assertEquals('finished', testElement.getAttribute('glif-animation-state'));
  });

  test(
      'No animations when --cr-animations-disabled is 1 with energy effect',
      async () => {
        document.body.style.setProperty('--cr-animations-disabled', '1');
        const testElement = createEntrypointButton();
        testElement.showContextMenuDescription = true;
        testElement.energyEffectAnimationEnabled = true;
        testElement.setAttribute('glif-animation-state', 'started');
        await microtasksFinished();

        // selector: The query selector for the element to check.
        // style_assertion: The expected styles for the element.
        const checks = [
          {
            selector: '#entrypoint',
            style_assertion: {
              'animation-name': 'none',
            },
          },
          {
            selector: '#entrypointIcon',
            style_assertion: {
              'animation-name': 'none',
            },
          },
          {
            selector: '#description',
            style_assertion: {
              'animation-name': 'none',
              'opacity': '1',
              'transform': 'none',
            },
          },
        ];

        for (const check of checks) {
          const element = $$(testElement, check.selector);
          assertTrue(!!element, `Expect an element with ${check.selector}`);
          const style = window.getComputedStyle(element);
          const actualStyles: Record<string, string> = {};
          for (const property of Object.keys(check.style_assertion)) {
            actualStyles[property] =
                style[property as keyof CSSStyleDeclaration] as unknown as
                string;
          }
          assertDeepEquals(
              actualStyles, check.style_assertion,
              `Expect ${check.selector} to have the following styles: ${
                  JSON.stringify(check.style_assertion)}, got: ${
                  JSON.stringify(actualStyles)}`);
        }

        // Clean up
        document.body.style.removeProperty('--cr-animations-disabled');
      });

  suite('SmartTabSharingEntrypoint', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        tabFaviconChipsToCoinsEnabled: true,
      });

      entrypointButton = createEntrypointButton();
      await microtasksFinished();
    });

    test('STS is active (with description)', async () => {
      entrypointButton.smartTabSharingActive = true;
      entrypointButton.showContextMenuDescription = true;

      await microtasksFinished();
      await entrypointButton.updateComplete;

      const entrypoint = $$(entrypointButton, '#entrypoint');
      assertTrue(!!entrypoint);
      assertEquals(
          entrypointButton.i18n('addContextTitle'),
          entrypoint.getAttribute('title'));

      // Main icon remains "+"
      const icon = $$(entrypointButton, '#entrypointIcon');
      assertTrue(!!icon);
      assertEquals('cr:add', icon.getAttribute('icon'));

      // STS icon is shown in coins slot
      const coinIcon = $$(entrypointButton, '.sts-active-coin');
      assertTrue(!!coinIcon);
      assertEquals('composebox:shareTabs', coinIcon.getAttribute('icon'));
      assertEquals(
          entrypointButton.i18n('stsMegaplusShareRelevantOpenTabs'),
          coinIcon.getAttribute('title'));

      // Favicon group is NOT shown
      const faviconGroup = $$(entrypointButton, 'composebox-favicon-group');
      assertFalse(!!faviconGroup);
    });

    test('STS is active (description hidden)', async () => {
      entrypointButton.smartTabSharingActive = true;
      entrypointButton.showContextMenuDescription = false;

      await microtasksFinished();
      await entrypointButton.updateComplete;

      const entrypoint = $$(entrypointButton, '#entrypoint');
      assertTrue(!!entrypoint);
      assertTrue(
          entrypoint.classList.contains('pill-button'));  // Pill button layout
      assertEquals(
          entrypointButton.i18n('addContextTitle'),
          entrypoint.getAttribute('title'));

      // Main icon remains "+"
      const icon = $$(entrypointButton, '#entrypointIcon');

      assertTrue(!!icon);
      assertEquals('cr:add', icon.getAttribute('icon'));

      // STS icon is shown in coins slot
      const coinIcon = $$(entrypointButton, '.sts-active-coin');
      assertTrue(!!coinIcon);
      assertEquals('composebox:shareTabs', coinIcon.getAttribute('icon'));
      assertEquals(
          entrypointButton.i18n('stsMegaplusShareRelevantOpenTabs'),
          coinIcon.getAttribute('title'));
    });

    test(
        'STS is inactive, description hidden, has tabs (show coins)',
        async () => {
          entrypointButton.smartTabSharingActive = false;
          entrypointButton.showContextMenuDescription = false;

          entrypointButton.sharedTabs = [
            {
              tabId: 1,
              url: 'https://example.com',
              title: 'Tab 1',
              showInCurrentTabChip: false,
              showInPreviousTabChip: false,
              lastActive: {internalValue: 0n},
            },
          ];
          await microtasksFinished();
          await entrypointButton.updateComplete;

          const entrypoint = $$(entrypointButton, '#entrypoint');
          assertTrue(!!entrypoint);
          assertFalse(entrypoint.classList.contains('sts-active'));
          assertTrue(entrypoint.classList.contains('pill-button'));

          // Main icon remains "+"
          const icon = $$(entrypointButton, '#entrypointIcon');
          assertTrue(!!icon);
          assertEquals('cr:add', icon.getAttribute('icon'));

          // Favicon group is shown
          const faviconGroup = $$(entrypointButton, 'composebox-favicon-group');
          assertTrue(!!faviconGroup);

          // STS icon is NOT shown
          const coinIcon = $$(entrypointButton, '.sts-active-coin');
          assertFalse(!!coinIcon);
        });
  });
});
