// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {MiddleSlotPromoElement} from 'chrome://new-tab-page/lazy_load.js';
import {PromoDismissAction} from 'chrome://new-tab-page/lazy_load.js';
import type {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, BrowserCommandProxy, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {PageRemote, Promo} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('NewTabPageMiddleSlotPromoTest', () => {
  let newTabPageHandler: TestMock<PageHandlerRemote>;
  let promoBrowserCommandHandler: TestMock<CommandHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;
  let middleSlotPromo: MiddleSlotPromoElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    newTabPageHandler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
    promoBrowserCommandHandler = installMock(
        CommandHandlerRemote,
        mock => BrowserCommandProxy.setInstance({handler: mock}));
  });

  function createPromo() {
    return {
      id: '7',
      logUrl: {
        url:
            'https://www.google.com/gen_204?ei=AsDMYoL9DtzVkPIP19ScaA&cad=i&id=19030295&ogprm=up&ct=16&prid=243',
      },
      middleSlotParts: [
        {image: {imageUrl: {url: 'https://image'}, target: {url: ''}}},
        {
          image: {
            imageUrl: {url: 'https://image'},
            target: {url: 'https://link'},
          },
        },
        {
          image: {
            imageUrl: {url: 'https://image'},
            target: {url: 'command:123'},
          },
        },
        {text: {text: 'text', color: 'red'}},
        {
          link: {
            url: {url: 'https://link'},
            text: 'link',
            color: 'green',
          },
        },
        {
          link: {
            url: {url: 'command:123'},
            text: 'command',
            color: 'blue',
          },
        },
      ],
    };
  }

  async function createMiddleSlotPromo(
      canShowPromo: boolean, hasPromoId: boolean = true) {
    promoBrowserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: canShowPromo}));

    middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);

    const promo = createPromo() as Promo;
    if (!hasPromoId) {
      promo.id = '';
    }
    callbackRouterRemote.setPromo(promo);
    await callbackRouterRemote.$.flushForTesting();

    if (canShowPromo) {
      await promoBrowserCommandHandler.whenCalled('canExecuteCommand');
      assertEquals(
          2, promoBrowserCommandHandler.getCallCount('canExecuteCommand'));
      await newTabPageHandler.whenCalled('onPromoRendered');
    } else {
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
    }
    await loaded;
  }

  async function createMiddleSlotPromoWithData() {
    await createMiddleSlotPromo(/*canShowPromo=*/ true, /*hasPromoId=*/ true);
  }

  function assertMiddleSlotPromoHasContent(hasContent: boolean) {
    assertEquals(
        hasContent, isVisible(middleSlotPromo.$.promoAndDismissContainer));
    assertEquals(hasContent, !!$$(middleSlotPromo, '#promoContainer'));
  }

  test('render canShowPromo=true', async () => {
    // Create the promo and get the container element.
    await createMiddleSlotPromoWithData();
    assertMiddleSlotPromoHasContent(true);
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assertTrue(!!promoContainer);

    // Verify that the promo container populated correctly.
    const parts = promoContainer.children;
    assertEquals(6, parts.length);
    const image = parts[0] as CrAutoImgElement;
    assertEquals('https://image', image.autoSrc);
    const imageWithLink = parts[1] as HTMLAnchorElement;
    assertEquals('https://link/', imageWithLink.href);
    assertEquals(
        'https://image',
        (imageWithLink.children[0] as CrAutoImgElement).autoSrc);
    const imageWithCommand = parts[2] as HTMLAnchorElement;
    assertEquals('', imageWithCommand.href);
    assertEquals(
        'https://image',
        (imageWithCommand.children[0] as CrAutoImgElement).autoSrc);
    const text = parts[3] as HTMLElement;
    assertEquals('text', text.innerText);
    const link = parts[4] as HTMLAnchorElement;
    assertEquals('https://link/', link.href);
    assertEquals('link', link.innerText);
    const command = parts[5] as HTMLAnchorElement;
    assertEquals('', command.href);
    assertEquals('command', command.text);
  });

  test('render canShowPromo=false', async () => {
    const canShowPromo = false;
    await createMiddleSlotPromo(canShowPromo);
    assertMiddleSlotPromoHasContent(canShowPromo);
  });

  test('clicking on command', async () => {
    await createMiddleSlotPromoWithData();
    promoBrowserCommandHandler.setResultFor(
        'executeCommand', Promise.resolve());
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assertTrue(!!promoContainer);

    async function testClick(el: HTMLElement) {
      promoBrowserCommandHandler.reset();
      el.click();
      // Make sure the command and click information are sent to the browser.
      const [expectedCommand, expectedClickInfo] =
          await promoBrowserCommandHandler.whenCalled('executeCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(Command.kUnknownCommand, expectedCommand);
      assertDeepEquals(
          {
            middleButton: false,
            altKey: false,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          expectedClickInfo);
    }

    const imageWithCommand = promoContainer.children[2] as HTMLElement;
    await testClick(imageWithCommand);
    const command = promoContainer.children[5] as HTMLElement;
    await testClick(command);
  });

  suite('middle slot promo dismissal', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        middleSlotPromoDismissalEnabled: true,
      });
    });

    async function clickDismissPromoButton(
        middleSlotPromo: MiddleSlotPromoElement) {
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(2, parts.length);
      const dismissPromoButton = parts[1] as HTMLElement;
      dismissPromoButton.click();
      await microtasksFinished();
    }

    test(`dismiss button doesn't show if there is no promo id`, async () => {
      const canShowPromo = true;
      const hasPromoId = false;
      await createMiddleSlotPromo(canShowPromo, hasPromoId);

      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(1, parts.length);
      assertEquals(parts[0]!.id, 'promoContainer');
    });

    test('clicking dismiss button dismisses promo', async () => {
      await createMiddleSlotPromoWithData();

      await clickDismissPromoButton(middleSlotPromo);

      assertEquals(1, newTabPageHandler.getCallCount('blocklistPromo'));
      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.DISMISS));
    });

    test('clicking undo button restores promo', async () => {
      // Dismiss the promo.
      await createMiddleSlotPromoWithData();
      await clickDismissPromoButton(middleSlotPromo);
      assertEquals(0, newTabPageHandler.getCallCount('undoBlocklistPromo'));
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));

      // Undo dismissal via toast.
      middleSlotPromo.$.undoDismissPromoButton.click();

      assertEquals(1, newTabPageHandler.getCallCount('undoBlocklistPromo'));
      assertTrue(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));
    });

    test('restores promo if undo command is fired via keyboard', async () => {
      // Dismiss the promo.
      await createMiddleSlotPromoWithData();
      await clickDismissPromoButton(middleSlotPromo);
      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));

      // Simulate 'ctrl+z' key combo (or meta+z for Mac).
      keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

      assertEquals(1, newTabPageHandler.getCallCount('undoBlocklistPromo'));
      assertTrue(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));
    });

    test('ignores undo command if no promo blocklisted', async () => {
      await createMiddleSlotPromoWithData();

      // Simulate 'ctrl+z' key combo (or meta+z for Mac).
      keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

      assertEquals(0, newTabPageHandler.getCallCount('undoBlocklistPromo'));
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));
    });

    test('setting promo data resurfaces promo after dismissal', async () => {
      // Dismiss the promo.
      await createMiddleSlotPromoWithData();
      await clickDismissPromoButton(middleSlotPromo);

      // Set promo data.
      callbackRouterRemote.setPromo(createPromo());
      await callbackRouterRemote.$.flushForTesting();

      // Assert that the promo resurfaces.
      assertTrue(isVisible(middleSlotPromo.$.promoAndDismissContainer));
    });
  });
});
