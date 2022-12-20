// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/cards.js';

import {CardsElement} from 'chrome://customize-chrome-side-panel.top-chrome/cards.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, ModuleSettings} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {assertNotStyle, assertStyle, installMock} from './test_support.js';

suite('CardsTest', () => {
  let customizeCards: CardsElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function setupTest(
      modules: ModuleSettings[], modulesVisible: boolean,
      modulesManaged: boolean) {
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
              visible, false);

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
          /*modulesVisible=*/ visible,
          /*modulesManaged=*/ false);

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
              /*modulesVisible=*/ visible,
              /*modulesManaged=*/ true);

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

  // TODO(crbug.com/1384258): Add metric related tests.
});
