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
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      canShowPromo: boolean,
      hasPromoId: boolean = true): Promise<MiddleSlotPromoElement> {
    promoBrowserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: canShowPromo}));

    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
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
    return middleSlotPromo;
  }

  function assertHasContent(
      hasContent: boolean, middleSlotPromo: MiddleSlotPromoElement) {
    assertEquals(
        hasContent, isVisible(middleSlotPromo.$.promoAndDismissContainer));
    assertEquals(hasContent, !!$$(middleSlotPromo, '#promoContainer'));
  }

  test(`render canShowPromo=true`, async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assertTrue(!!promoContainer);
    const parts = promoContainer.children;
    assertEquals(6, parts.length);

    const image = parts[0] as CrAutoImgElement;
    const imageWithLink = parts[1] as HTMLAnchorElement;
    const imageWithCommand = parts[2] as HTMLAnchorElement;
    const text = parts[3] as HTMLElement;
    const link = parts[4] as HTMLAnchorElement;
    const command = parts[5] as HTMLAnchorElement;

    assertEquals('https://image', image.autoSrc);

    assertEquals('https://link/', imageWithLink.href);
    assertEquals(
        'https://image',
        (imageWithLink.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('', imageWithCommand.href);
    assertEquals(
        'https://image',
        (imageWithCommand.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('text', text.innerText);

    assertEquals('https://link/', link.href);
    assertEquals('link', link.innerText);

    assertEquals('', command.href);
    assertEquals('command', command.text);
  });

  test('render canShowPromo=false', async () => {
    const canShowPromo = false;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
  });

  test('clicking on command', async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    promoBrowserCommandHandler.setResultFor(
        'executeCommand', Promise.resolve());
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assertTrue(!!promoContainer);
    const imageWithCommand = promoContainer.children[2] as HTMLElement;
    const command = promoContainer.children[5] as HTMLElement;

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

    await testClick(imageWithCommand);
    await testClick(command);
  });

  suite('middle slot promo dismissal', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        middleSlotPromoDismissalEnabled: true,
      });
    });

    test(`dismiss button doesn't show if there is no promo id`, async () => {
      const canShowPromo = true;
      const hasPromoId = false;
      const middleSlotPromo =
          await createMiddleSlotPromo(canShowPromo, hasPromoId);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(1, parts.length);

      const promoContainer = parts[0] as HTMLElement;
      assertEquals(6, promoContainer.children.length);
    });

    test(`clicking dismiss button dismisses promo`, async () => {
      const canShowPromo = true;
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(2, parts.length);

      const dismissPromoButton = parts[1] as HTMLElement;
      dismissPromoButton.click();
      assertTrue(middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.DISMISS));
    });

    test(`clicking undo button restores promo`, async () => {
      const canShowPromo = true;
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(2, parts.length);

      const dismissPromoButton = parts[1] as HTMLElement;
      dismissPromoButton.click();
      assertEquals(true, middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.DISMISS));

      middleSlotPromo.$.undoDismissPromoButton.click();
      assertEquals(false, middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));
    });

    test(`setting promo data resurfaces promo after dismissal`, async () => {
      const canShowPromo = true;
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(2, parts.length);

      callbackRouterRemote.setPromo(null);
      await callbackRouterRemote.$.flushForTesting();
      assertEquals(true, middleSlotPromo.$.promoAndDismissContainer.hidden);

      const promo = createPromo();
      callbackRouterRemote.setPromo(promo as Promo);
      await callbackRouterRemote.$.flushForTesting();
      assertEquals(false, middleSlotPromo.$.promoAndDismissContainer.hidden);
    });
  });

  suite('mobilePromoEnabled', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        mobilePromoEnabled: true,
      });
    });

    test(`mobile promo hides if default promo renders`, async () => {
      const canShowPromo = true;
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertTrue(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertFalse(isVisible(middleSlotPromo.$.mobilePromo));
    });

    test(`mobile promo shows if default promo doesn't render`, async () => {
      const canShowPromo = false;
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertTrue(isVisible(middleSlotPromo.$.mobilePromo));
    });

    test(
        `mobile promo hides if default promo doesn't render and no qr code`,
        async () => {
          const canShowPromo = false;
          newTabPageHandler.setResultFor(
              'getMobilePromoQrCode', Promise.resolve({qrCode: ''}));
          const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
          assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
          assertFalse(isVisible(middleSlotPromo.$.mobilePromo));
        });

    test(`mobile promo shows if it gets a QR code later`, async () => {
      const canShowPromo = false;
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: ''}));
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      const mobilePromo = middleSlotPromo.$.mobilePromo;
      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertFalse(isVisible(mobilePromo));
      mobilePromo.dispatchEvent(new CustomEvent('qr-code-changed', {
        bubbles: true,
        composed: true,
        detail: {value: 'abc'},
      }));
      await microtasksFinished();

      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertTrue(isVisible(mobilePromo));
    });

    test(`mobile promo hides if QR code gets removed later`, async () => {
      const canShowPromo = false;
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      const mobilePromo = middleSlotPromo.$.mobilePromo;
      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertTrue(isVisible(mobilePromo));
      mobilePromo.dispatchEvent(new CustomEvent('qr-code-changed', {
        bubbles: true,
        composed: true,
        detail: {value: ''},
      }));
      await microtasksFinished();

      assertFalse(isVisible(middleSlotPromo.$.promoAndDismissContainer));
      assertFalse(isVisible(mobilePromo));
    });
  });
});
