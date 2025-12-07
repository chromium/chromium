// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/cards.js';

import type {ButtonLabelElement} from 'chrome://customize-chrome-side-panel.top-chrome/button_label.js';
import type {CardsElement} from 'chrome://customize-chrome-side-panel.top-chrome/cards.js';
import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote, ModuleSettings} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, installMock} from './test_support.js';

suite('CardsTest', () => {
  let customizeCards: CardsElement;
  let metrics: MetricsTracker;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(() => {
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
    await microtasksFinished();
  }

  function getToggleElement(): CrToggleElement {
    return customizeCards.$['showToggleContainer'].querySelector('cr-toggle')!;
  }

  function getCollapseElement() {
    return customizeCards.shadowRoot.querySelector('cr-collapse')!;
  }

  function getCardsMap(): Map<string, HTMLElement> {
    const elements: HTMLElement[] = Array.from(
        customizeCards.shadowRoot.querySelectorAll<HTMLElement>('.card'));
    return Object.freeze(new Map(elements.map(cardEl => {
      const cardNameEl =
          cardEl.querySelector<ButtonLabelElement>('.card-label');
      assertTrue(!!cardNameEl);
      assertNotEquals(null, cardNameEl.label);
      return [cardNameEl.label, cardEl];
    })));
  }

  function assertCardCheckedStatus(
      cards: Map<string, HTMLElement>, name: string, checked: boolean) {
    assertTrue(cards.has(name));
    const checkbox: CrCheckboxElement =
        cards.get(name)!.querySelector('cr-checkbox')!;
    assertEquals(checked, checkbox.checked);
  }

  function assertCardVisibility(
      cards: Map<string, HTMLElement>, name: string, visible: boolean) {
    assertTrue(cards.has(name));
    assertEquals(visible, isVisible(cards.get(name)!));
  }

  [true, false].forEach(visible => {
    test(
        `creating element shows correctly for cards visibility '${visible}'`,
        async () => {
          // Arrange & Act.
          await setupTest(
              [
                {
                  id: 'foo',
                  name: 'foo name',
                  description: null,
                  enabled: true,
                  visible: true,
                },
                {
                  id: 'bar',
                  name: 'bar name',
                  description: 'bar description',
                  enabled: true,
                  visible: true,
                },
                {
                  id: 'baz',
                  name: 'baz name',
                  description: null,
                  enabled: false,
                  visible: true,
                },
              ],
              /*modulesManaged=*/ false,
              /*modulesVisible=*/ visible);

          // Assert.
          assertEquals(visible, getToggleElement().checked);
          const policyIndicator =
              customizeCards.shadowRoot.querySelector('cr-policy-indicator');
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
            {
              id: 'foo',
              name: 'foo name',
              description: 'foo description',
              enabled: true,
              visible: true,
            },
            {
              id: 'bar',
              name: 'bar name',
              description: null,
              enabled: false,
              visible: true,
            },
          ],
          /*modulesManaged=*/ false,
          /*modulesVisible=*/ visible);

      assertEquals(visible, getCollapseElement().opened);
      getToggleElement().click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Assert.
      assertEquals(!visible, getToggleElement().checked);
      assertEquals(!visible, getCollapseElement().opened);
      const cards = getCardsMap();
      assertCardCheckedStatus(cards, 'foo name', true);
      assertCardCheckedStatus(cards, 'bar name', false);
    });

    test(
        `toggling 'Show cards' ${toggleState} via its container works`,
        async () => {
          await setupTest(
              [
                {
                  id: 'foo',
                  name: 'foo name',
                  description: null,
                  enabled: true,
                  visible: true,
                },
                {
                  id: 'bar',
                  name: 'bar name',
                  description: 'bar description',
                  enabled: false,
                  visible: true,
                },
              ],
              /*modulesManaged=*/ false,
              /*modulesVisible=*/ visible);

          assertEquals(visible, getCollapseElement().opened);
          customizeCards.$.showToggleContainer.click();
          await callbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Assert.
          assertEquals(!visible, getToggleElement().checked);
          assertEquals(!visible, getCollapseElement().opened);
          const cards = getCardsMap();
          assertCardCheckedStatus(cards, 'foo name', true);
          assertCardCheckedStatus(cards, 'bar name', false);
        });

    test(
        `Policy disables toggling 'Show cards' when cards visibility is ${
            visible}`,
        async () => {
          await setupTest(
              [
                {
                  id: 'foo',
                  name: 'foo name',
                  description: null,
                  enabled: true,
                  visible: true,
                },
                {
                  id: 'bar',
                  name: 'bar name',
                  description: 'bar description',
                  enabled: false,
                  visible: true,
                },
              ],
              /*modulesManaged=*/ true,
              /*modulesVisible=*/ visible);

          customizeCards.$.showToggleContainer.click();
          await callbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Assert.
          assertEquals(visible, getToggleElement().checked);
        });

    test(
        `Policy disables actionable elements when cards visibility is ${
            visible}`,
        async () => {
          await setupTest(
              [
                {
                  id: 'foo',
                  name: 'foo name',
                  description: null,
                  enabled: true,
                  visible: true,
                },
                {
                  id: 'bar',
                  name: 'bar name',
                  description: 'bar description',
                  enabled: false,
                  visible: true,
                },
              ],
              /*modulesManaged=*/ true,
              /*modulesVisible=*/ visible);

          const policyIndicator =
              customizeCards.shadowRoot.querySelector('cr-policy-indicator');
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

  test(
      'cards visiblity depends on their associated module settings',
      async () => {
        // Arrange/Act.
        await setupTest(
            [
              {
                id: 'foo',
                name: 'foo name',
                description: null,
                enabled: true,
                visible: true,
              },
              {
                id: 'bar',
                name: 'bar name',
                description: 'bar description',
                enabled: false,
                visible: false,
              },
            ],
            /*modulesManaged=*/ false,
            /*modulesVisible=*/ true);
        await microtasksFinished();

        // Assert.
        const cards = getCardsMap();
        assertCardVisibility(cards, 'foo name', true);
        assertCardVisibility(cards, 'bar name', false);
      });


  test(`cards can be disabled/enabled via their checkbox`, async () => {
    // Arrange & Act.
    await setupTest(
        [
          {
            id: 'foo',
            name: 'foo name',
            description: null,
            enabled: true,
            visible: true,
          },
        ],
        /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);

    const cards = getCardsMap();
    const fooCheckbox = cards.get('foo name')!.querySelector('cr-checkbox');
    assertTrue(!!fooCheckbox);

    // Act.
    fooCheckbox.click();
    await fooCheckbox.updateComplete;

    // Assert.
    assertDeepEquals(['foo', true], handler.getArgs('setModuleDisabled')[0]);
    assertCardCheckedStatus(cards, 'foo name', false);
    assertEquals(1, metrics.count('NewTabPage.Modules.Disabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Disabled.Customize', 'foo'));

    // Act.
    fooCheckbox.click();
    await fooCheckbox.updateComplete;

    // Assert.
    assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);
    assertCardCheckedStatus(cards, 'foo name', true);
    assertEquals(1, metrics.count('NewTabPage.Modules.Enabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Enabled.Customize', 'foo'));
  });

  test(`cards can be disabled/enabled via their label`, async () => {
    // Arrange & Act.
    await setupTest(
        [
          {
            id: 'foo',
            name: 'foo name',
            description: null,
            enabled: true,
            visible: true,
          },
        ],
        /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);

    const cards = getCardsMap();
    const fooCard = cards.get('foo name')!;
    assertTrue(!!fooCard);
    const fooCheckbox = cards.get('foo name')!.querySelector('cr-checkbox');
    assertTrue(!!fooCheckbox);

    // Act.
    (fooCard).click();
    await fooCheckbox.updateComplete;

    // Assert.
    assertDeepEquals(['foo', true], handler.getArgs('setModuleDisabled')[0]);
    assertCardCheckedStatus(cards, 'foo name', false);
    assertEquals(1, metrics.count('NewTabPage.Modules.Disabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Disabled.Customize', 'foo'));

    // Act.
    fooCheckbox.click();
    await fooCheckbox.updateComplete;

    // Assert.
    assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);
    assertCardCheckedStatus(cards, 'foo name', true);
    assertEquals(1, metrics.count('NewTabPage.Modules.Enabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Enabled.Customize', 'foo'));
  });

  test('only animates after initialization', async () => {
    // Arrange.
    customizeCards = document.createElement('customize-chrome-cards');
    document.body.appendChild(customizeCards);

    // Assert (no animation before initialize).
    assertTrue(getCollapseElement().noAnimation);

    // Act (initialize).
    callbackRouterRemote.setModulesSettings(
        [{
          id: 'foo',
          name: 'Foo',
          description: null,
          enabled: true,
          visible: true,
        }],
        /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Assert (animation after initialize).
    assertFalse(getCollapseElement().noAnimation);

    // Act (update).
    callbackRouterRemote.setModulesSettings(
        [{
          id: 'bar',
          name: 'Bar',
          description: null,
          enabled: true,
          visible: true,
        }],
        /*modulesManaged=*/ false,
        /*modulesVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Assert (still animation after update).
    assertFalse(getCollapseElement().noAnimation);
  });

  suite('Metrics', () => {
    test('Clicking show cards toggle sets metric', async () => {
      getToggleElement().click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_CARDS_TOGGLE_CLICKED));
    });
  });
});
