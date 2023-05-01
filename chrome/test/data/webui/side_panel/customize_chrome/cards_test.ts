// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/cards.js';

import {CardsElement} from 'chrome://customize-chrome-side-panel.top-chrome/cards.js';
import {CartHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_cart.mojom-webui.js';
import {ChromeCartProxy} from 'chrome://customize-chrome-side-panel.top-chrome/chrome_cart_proxy.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, ModuleSettings} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, installMock} from './test_support.js';

suite('CardsTest', () => {
  let customizeCards: CardsElement;
  let metrics: MetricsTracker;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function setupTest(
      modules: ModuleSettings[], modulesManaged: boolean,
      modulesVisible: boolean) {
    callbackRouterRemote.setModulesSettings(
        modules, modulesManaged, modulesVisible);

    customizeCards = document.createElement('customize-chrome-cards');
    document.body.appendChild(customizeCards);
    await handler.whenCalled('updateModulesSettings');
    await waitAfterNextRender(customizeCards);
  }

  function getToggleElement(): CrToggleElement {
    return customizeCards.$['showToggleContainer']!.querySelector('cr-toggle')!;
  }

  function getCollapseElement(): IronCollapseElement {
    return customizeCards.shadowRoot!.querySelector('iron-collapse')!;
  }

  function getCardsMap(): Map<string, HTMLElement> {
    const elements: HTMLElement[] = Array.from(
        customizeCards.shadowRoot!.querySelectorAll<HTMLElement>('.card'));
    return Object.freeze(new Map(elements.map(cardEl => {
      assertNotEquals(null, cardEl.firstChild);
      assertNotEquals(null, cardEl.firstChild!.textContent);
      return [cardEl.firstChild!.textContent!, cardEl];
    })));
  }

  function assertCardCheckedStatus(
      cards: Map<string, HTMLElement>, name: string, checked: boolean) {
    assertTrue(cards.has(name));
    const checkbox: CrCheckboxElement|null =
        cards.get(name)!.querySelector('cr-checkbox')!;
    assertEquals(checked, checkbox.checked);
  }

  [true, false].forEach(visible => {
    test(
        `creating element shows correctly for cards visibility '${visible}'`,
        async () => {
          // Arrange & Act.
          await setupTest(
              [
                {id: 'foo', name: 'foo name', enabled: true},
                {id: 'bar', name: 'bar name', enabled: true},
                {id: 'baz', name: 'baz name', enabled: false},
              ],
              /*modulesManaged=*/ false,
              /*modulesVisible=*/ visible);

          // Assert.
          assertEquals(visible, getToggleElement().checked);
          const policyIndicator =
              customizeCards.shadowRoot!.querySelector('cr-policy-indicator');
          assertStyle(policyIndicator!, 'display', 'none');

          const collapseElement = getCollapseElement();
          assertEquals(visible, collapseElement.opened);

          const cards = getCardsMap();
          if (visible) {
            assertCardCheckedStatus(cards, 'foo name', true);
            assertCardCheckedStatus(cards, 'bar name', true);
            assertCardCheckedStatus(cards, 'baz name', false);
          }
        });

    const toggleState = visible ? 'on' : 'off';
    test(`toggling 'Show cards' ${toggleState} shows correctly`, async () => {
      await setupTest(
          [
            {id: 'foo', name: 'foo name', enabled: true},
            {id: 'bar', name: 'bar name', enabled: false},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ visible);

      assertEquals(visible, getCollapseElement().opened);
      getToggleElement().click();
      await callbackRouterRemote.$.flushForTesting();
      await waitAfterNextRender(customizeCards);

      // Assert.
      assertEquals(!visible, getToggleElement().checked);
      assertEquals(!visible, getCollapseElement().opened);
      const cards = getCardsMap();
      assertCardCheckedStatus(cards, 'foo name', true);
      assertCardCheckedStatus(cards, 'bar name', false);
    });

    test(
        `Policy disables actionable elements when cards visibility is ${
            visible}`,
        async () => {
          await setupTest(
              [
                {id: 'foo', name: 'foo name', enabled: true},
                {id: 'bar', name: 'bar name', enabled: false},
              ],
              /*modulesManaged=*/ true,
              /*modulesVisible=*/ visible);

          const policyIndicator =
              customizeCards.shadowRoot!.querySelector('cr-policy-indicator');
          assertNotStyle(policyIndicator!, 'display', 'none');
          assertTrue(getToggleElement().disabled);
          const cards = getCardsMap();
          cards.forEach(cardEl => {
            const checkbox: CrCheckboxElement =
                cardEl.querySelector('cr-checkbox')!;
            assertTrue(checkbox.disabled);
          });
          assertCardCheckedStatus(cards, 'foo name', true);
          assertCardCheckedStatus(cards, 'bar name', false);
        });
  });

  test(`cards can be disabled and enabled`, async () => {
    // Arrange & Act.
    await setupTest(
        [
          {id: 'foo', name: 'foo name', enabled: true},
        ],
        /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);

    const cards = getCardsMap();
    const fooCheckbox = cards.get('foo name')!.querySelector('cr-checkbox')!;

    // Act.
    fooCheckbox.click();

    // Assert.
    assertDeepEquals(['foo', true], handler.getArgs('setModuleDisabled')[0]);
    assertCardCheckedStatus(cards, 'foo name', false);
    assertEquals(1, metrics.count('NewTabPage.Modules.Disabled', 'foo'));

    // Act.
    fooCheckbox.click();

    // Assert.
    assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);
    assertCardCheckedStatus(cards, 'foo name', true);
    assertEquals(1, metrics.count('NewTabPage.Modules.Enabled', 'foo'));
  });

  suite('Chrome Cart', () => {
    let cartHandler: TestMock<CartHandlerRemote>;

    suiteSetup(() => {
      cartHandler = installMock(CartHandlerRemote, ChromeCartProxy.setHandler);
    });

    [true, false].forEach(visible => {
      test(`Discount option ${(visible ? '' : 'not ')} visible`, async () => {
        // Arrange.
        cartHandler.setResultFor(
            'getDiscountToggleVisible',
            Promise.resolve({toggleVisible: visible}));
        cartHandler.setResultFor(
            'getDiscountEnabled', Promise.resolve({enabled: false}));
        await setupTest(
            [
              {id: 'chrome_cart', name: 'Chrome Cart', enabled: true},
            ],
            /*modulesManaged=*/ false,
            /*modulesVisible=*/ visible);

        // Assert.
        assertEquals(visible, getToggleElement().checked);
        const cards = getCardsMap();
        assertCardCheckedStatus(cards, 'Chrome Cart', true);
        if (visible) {
          assertCardCheckedStatus(
              cards, loadTimeData.getString('modulesCartDiscountConsentAccept'),
              false);
        }
        const cardOptions =
            customizeCards.shadowRoot!.querySelectorAll('.card-option-name');
        assertEquals(visible ? 1 : 0, cardOptions.length);
      });
    });

    test(`discount checkbox sets discount status`, async () => {
      // Arrange.
      cartHandler.setResultFor(
          'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
      cartHandler.setResultFor(
          'getDiscountEnabled', Promise.resolve({enabled: true}));

      await setupTest(
          [
            {id: 'chrome_cart', name: 'Chrome Cart', enabled: true},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ true);

      // Act.
      const cartCardOptionName =
          customizeCards.shadowRoot!.querySelector('.card-option-name')!;
      const discountCheckbox: CrCheckboxElement =
          cartCardOptionName.nextElementSibling! as CrCheckboxElement;
      discountCheckbox.click();

      // Assert.
      assertEquals(1, cartHandler.getCallCount('setDiscountEnabled'));
      assertDeepEquals(false, cartHandler.getArgs('setDiscountEnabled')[0]);
    });

    test(`Unchecking cart card hides discount option`, async () => {
      // Arrange.
      cartHandler.setResultFor(
          'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
      cartHandler.setResultFor(
          'getDiscountEnabled', Promise.resolve({enabled: true}));

      await setupTest(
          [
            {id: 'chrome_cart', name: 'Chrome Cart', enabled: true},
            {id: 'bar', name: 'bar name', enabled: false},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ true);

      assertTrue(getToggleElement().checked);
      assertTrue(getCollapseElement().opened);
      let cards = getCardsMap();
      assertCardCheckedStatus(cards, 'Chrome Cart', true);
      assertCardCheckedStatus(
          cards, loadTimeData.getString('modulesCartDiscountConsentAccept'),
          true);
      assertCardCheckedStatus(cards, 'bar name', false);

      const cartCardCheckbox =
          cards.get('Chrome Cart')!.querySelector('cr-checkbox')!;
      cartCardCheckbox.click();
      await handler.whenCalled('setModuleDisabled');
      callbackRouterRemote.setModulesSettings(
          [
            {id: 'chrome_cart', name: 'Chrome Cart', enabled: false},
            {id: 'bar', name: 'bar name', enabled: false},
          ],
          false, true);
      await callbackRouterRemote.$.flushForTesting();
      await waitAfterNextRender(customizeCards);

      const cartCardOptionName =
          customizeCards.shadowRoot!.querySelector('.card-option-name')!;
      assertFalse(isVisible(cartCardOptionName));

      cards = getCardsMap();
      assertCardCheckedStatus(cards, 'Chrome Cart', false);
      assertCardCheckedStatus(cards, 'bar name', false);
      assertEquals(
          1, metrics.count('NewTabPage.Modules.Disabled', 'chrome_cart'));
    });
  });

  test('only animates after initialization', async () => {
    // Arrange.
    customizeCards = document.createElement('customize-chrome-cards');
    document.body.appendChild(customizeCards);

    // Assert (no animation before initialize).
    assertTrue(getCollapseElement().noAnimation!);

    // Act (initialize).
    callbackRouterRemote.setModulesSettings(
        [{id: 'foo', name: 'Foo', enabled: true}], /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Assert (animation after initialize).
    assertFalse(getCollapseElement().noAnimation!);

    // Act (update).
    callbackRouterRemote.setModulesSettings(
        [{id: 'bar', name: 'Bar', enabled: true}], /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Assert (still animation after update).
    assertFalse(getCollapseElement().noAnimation!);
  });

  suite('History Cluster', () => {
    let cartHandler: TestMock<CartHandlerRemote>;

    suiteSetup(() => {
      cartHandler = installMock(CartHandlerRemote, ChromeCartProxy.setHandler);
    });

    [true, false].forEach(visible => {
      test(`Cart option ${(visible ? '' : 'not ')} visible`, async () => {
        // Arrange.
        cartHandler.setResultFor(
            'getDiscountToggleVisible',
            Promise.resolve({toggleVisible: false}));
        cartHandler.setResultFor(
            'getDiscountEnabled', Promise.resolve({enabled: false}));
        cartHandler.setResultFor(
            'getCartFeatureEnabled', Promise.resolve({enabled: true}));
        loadTimeData.overrideValues({'showCartInQuestModuleSetting': visible});

        await setupTest(
            [
              {id: 'history_clusters', name: 'History Cluster', enabled: true},
            ],
            /*modulesManaged=*/ false,
            /*modulesVisible=*/ true);

        // Assert.
        assertEquals(true, getToggleElement().checked);
        const cards = getCardsMap();
        assertCardCheckedStatus(cards, 'History Cluster', true);
        if (visible) {
          assertCardCheckedStatus(
              cards, loadTimeData.getString('modulesCartSentence'), true);
        }

        const cardOptions =
            customizeCards.shadowRoot!.querySelectorAll('.card-option-name');
        assertEquals(visible ? 1 : 0, cardOptions.length);
        const cartOption =
            customizeCards.shadowRoot!.querySelector('#cartOption');
        assertEquals(!!cartOption, visible);
      });
    });

    test(`cart checkbox sets cart status`, async () => {
      // Arrange.
      cartHandler.setResultFor(
          'getDiscountToggleVisible', Promise.resolve({toggleVisible: false}));
      cartHandler.setResultFor(
          'getDiscountEnabled', Promise.resolve({enabled: false}));
      cartHandler.setResultFor(
          'getCartFeatureEnabled', Promise.resolve({enabled: true}));
      loadTimeData.overrideValues({'showCartInQuestModuleSetting': true});

      await setupTest(
          [
            {id: 'history_clusters', name: 'History Cluster', enabled: true},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ true);

      // Act.
      const cartCardOptionName =
          customizeCards.shadowRoot!.querySelector('#cartOption')!;
      const cartCheckbox: CrCheckboxElement =
          cartCardOptionName.nextElementSibling! as CrCheckboxElement;
      cartCheckbox.click();

      // Assert.
      assertEquals(1, handler.getCallCount('setModuleDisabled'));
      assertDeepEquals(
          'chrome_cart', handler.getArgs('setModuleDisabled')[0][0]);
      assertDeepEquals(true, handler.getArgs('setModuleDisabled')[0][1]);

      // Act.
      cartCheckbox.click();

      // Assert.
      assertEquals(2, handler.getCallCount('setModuleDisabled'));
      assertDeepEquals(
          'chrome_cart', handler.getArgs('setModuleDisabled')[1][0]);
      assertDeepEquals(false, handler.getArgs('setModuleDisabled')[1][1]);
    });

    [true, false].forEach(visible => {
      test(`Discount option ${(visible ? '' : 'not ')} visible`, async () => {
        // Arrange.
        cartHandler.setResultFor(
            'getDiscountToggleVisible',
            Promise.resolve({toggleVisible: visible}));
        cartHandler.setResultFor(
            'getDiscountEnabled', Promise.resolve({enabled: false}));
        cartHandler.setResultFor(
            'getCartFeatureEnabled', Promise.resolve({enabled: true}));
        loadTimeData.overrideValues({'showCartInQuestModuleSetting': true});

        await setupTest(
            [
              {id: 'history_clusters', name: 'History Cluster', enabled: true},
            ],
            /*modulesManaged=*/ false,
            /*modulesVisible=*/ true);

        // Assert.
        assertEquals(true, getToggleElement().checked);
        const cards = getCardsMap();
        assertCardCheckedStatus(cards, 'History Cluster', true);
        assertCardCheckedStatus(
            cards, loadTimeData.getString('modulesCartSentence'), true);
        const cardOptions =
            customizeCards.shadowRoot!.querySelectorAll('.card-option-name');
        assertEquals(visible ? 2 : 1, cardOptions.length);
        const discountOption =
            customizeCards.shadowRoot!.querySelector('#discountOption');
        assertEquals(!!discountOption, visible);
      });
    });

    test(`discount checkbox sets discount status`, async () => {
      // Arrange.
      cartHandler.setResultFor(
          'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
      cartHandler.setResultFor(
          'getDiscountEnabled', Promise.resolve({enabled: true}));
      cartHandler.setResultFor(
          'getCartFeatureEnabled', Promise.resolve({enabled: true}));
      loadTimeData.overrideValues({'showCartInQuestModuleSetting': true});

      await setupTest(
          [
            {id: 'history_clusters', name: 'History Cluster', enabled: true},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ true);

      // Act.
      const discountCardOptionName =
          customizeCards.shadowRoot!.querySelector('#discountOption')!;
      const discountCheckbox: CrCheckboxElement =
          discountCardOptionName.nextElementSibling! as CrCheckboxElement;
      discountCheckbox.click();

      // Assert.
      assertEquals(1, cartHandler.getCallCount('setDiscountEnabled'));
      assertDeepEquals(false, cartHandler.getArgs('setDiscountEnabled')[0]);

      // Act.
      discountCheckbox.click();

      // Assert.
      assertEquals(2, cartHandler.getCallCount('setDiscountEnabled'));
      assertDeepEquals(true, cartHandler.getArgs('setDiscountEnabled')[1]);
    });

    test(`Unchecking cart option hides discount option`, async () => {
      // Arrange.
      cartHandler.setResultFor(
          'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
      cartHandler.setResultFor(
          'getDiscountEnabled', Promise.resolve({enabled: true}));
      cartHandler.setResultFor(
          'getCartFeatureEnabled', Promise.resolve({enabled: true}));
      loadTimeData.overrideValues({'showCartInQuestModuleSetting': true});

      await setupTest(
          [
            {id: 'history_clusters', name: 'History Cluster', enabled: true},
            {id: 'bar', name: 'bar name', enabled: false},
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ true);

      // Assert.
      assertTrue(getToggleElement().checked);
      assertTrue(getCollapseElement().opened);
      let cards = getCardsMap();
      assertCardCheckedStatus(cards, 'History Cluster', true);
      assertCardCheckedStatus(
          cards, loadTimeData.getString('modulesCartSentence'), true);
      assertCardCheckedStatus(
          cards, loadTimeData.getString('modulesCartDiscountConsentAccept'),
          true);
      assertCardCheckedStatus(cards, 'bar name', false);

      // Act.
      const cartCardCheckbox =
          cards.get(loadTimeData.getString(
              'modulesCartSentence'))!.querySelector('cr-checkbox')!;
      cartCardCheckbox.click();
      await handler.whenCalled('setModuleDisabled');
      await waitAfterNextRender(customizeCards);

      // Assert.
      const discountCardOptionName =
          customizeCards.shadowRoot!.querySelector('#discountOption')!;
      assertFalse(isVisible(discountCardOptionName));
      cards = getCardsMap();
      assertCardCheckedStatus(cards, 'History Cluster', true);
      assertCardCheckedStatus(
          cards, loadTimeData.getString('modulesCartSentence'), false);
      assertCardCheckedStatus(cards, 'bar name', false);
    });

    test(
        `Unchecking history module hides both cart option and discount option`,
        async () => {
          // Arrange.
          cartHandler.setResultFor(
              'getDiscountToggleVisible',
              Promise.resolve({toggleVisible: true}));
          cartHandler.setResultFor(
              'getDiscountEnabled', Promise.resolve({enabled: true}));
          cartHandler.setResultFor(
              'getCartFeatureEnabled', Promise.resolve({enabled: true}));
          loadTimeData.overrideValues({'showCartInQuestModuleSetting': true});

          await setupTest(
              [
                {
                  id: 'history_clusters',
                  name: 'History Cluster',
                  enabled: true,
                },
                {id: 'bar', name: 'bar name', enabled: false},
              ],
              /*modulesManaged=*/ false,
              /*modulesVisible=*/ true);

          // Assert.
          assertTrue(getToggleElement().checked);
          assertTrue(getCollapseElement().opened);
          let cards = getCardsMap();
          assertCardCheckedStatus(cards, 'History Cluster', true);
          assertCardCheckedStatus(
              cards, loadTimeData.getString('modulesCartSentence'), true);
          assertCardCheckedStatus(
              cards, loadTimeData.getString('modulesCartDiscountConsentAccept'),
              true);
          assertCardCheckedStatus(cards, 'bar name', false);

          // Act.
          const historyCardCheckbox =
              cards.get('History Cluster')!.querySelector('cr-checkbox')!;
          historyCardCheckbox.click();
          await handler.whenCalled('setModuleDisabled');
          await waitAfterNextRender(customizeCards);

          // Assert.
          const discountCardOptionName =
              customizeCards.shadowRoot!.querySelector('#discountOption')!;
          assertFalse(isVisible(discountCardOptionName));
          const cartCardOptionName =
              customizeCards.shadowRoot!.querySelector('#cartOption')!;
          assertFalse(isVisible(cartCardOptionName));
          cards = getCardsMap();
          assertCardCheckedStatus(cards, 'History Cluster', false);
          assertCardCheckedStatus(cards, 'bar name', false);
        });
  });
});
