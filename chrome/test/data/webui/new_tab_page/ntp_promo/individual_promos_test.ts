// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import './ntp_promo_test_common.js';

import type {IndividualPromosElement} from 'chrome://new-tab-page/lazy_load.js';
import {getTrustedHTML} from 'chrome://new-tab-page/new_tab_page.js';
import type {NtpPromo as Promo} from 'chrome://new-tab-page/new_tab_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestNtpPromoProxy} from './ntp_promo_test_common.js';

suite('IndividualPromosTest', () => {
  let testProxy: TestNtpPromoProxy;
  let individualPromos: IndividualPromosElement;

  const promo: Promo = {
    id: 'promo0',
    iconName: '',
    bodyText: 'body text',
    buttonText: 'button text',
  };

  /**
   * Waits for the current frame to render, which queues intersection events,
   * and then waits for the intersection events to propagate to listeners, which
   * triggers visibility messages.
   *
   * This takes a total of two frames. A single frame will cause the layout to
   * be updated, but will not actually propagate the events.
   */
  async function waitForVisibilityEvents() {
    await waitAfterNextRender(individualPromos);
    return waitAfterNextRender(individualPromos);
  }

  function getContainer(): HTMLElement {
    return individualPromos.$.promos;
  }

  function getPromoCount(): number {
    return getContainer().querySelectorAll('#promo').length;
  }

  function getPromoAt(index: number): HTMLElement {
    return getContainer().querySelectorAll<HTMLElement>('#promo')[index]!;
  }

  setup(() => {
    testProxy = TestNtpPromoProxy.install();

    document.body.innerHTML = getTrustedHTML`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='bodyText'>This is some body text</p>
    </div>`;

    individualPromos = document.createElement('individual-promos');
    individualPromos.id = 'individualPromos';
    document.querySelector<HTMLElement>('#container')!.insertBefore(
        individualPromos, document.querySelector<HTMLElement>('#bodyText'));

    return waitForVisibilityEvents();
  });

  test('promo starts invisible', () => {
    assertFalse(isVisible(individualPromos), 'promo should not start visible');
  });

  test('set no promos hides promo container', async () => {
    individualPromos.onSetPromo(null);
    await waitForVisibilityEvents();
    assertFalse(isVisible(individualPromos));
  });


  // Check that the default number of showing promos.
  test('show single promo', async () => {
    individualPromos.onSetPromo(promo);
    await waitForVisibilityEvents();
    assertTrue(
        isVisible(individualPromos), 'promo frame should become visible');
    assertEquals(1, testProxy.getHandler().getCallCount('onPromoShown'));
    assertDeepEquals(
        [promo.id], testProxy.getHandler().getArgs('onPromoShown'));
    assertEquals(1, getPromoCount());
  });

  test('promo details', async () => {
    individualPromos.onSetPromo(promo);
    await waitForVisibilityEvents();
    const current = getPromoAt(0);
    assertEquals(
        current.querySelector<HTMLElement>('#bodyText')!.innerText,
        promo.bodyText);
    assertEquals(current.ariaLabel, promo.buttonText);
  });

  test('press button', async () => {
    individualPromos.onSetPromo(promo);
    await waitForVisibilityEvents();
    getPromoAt(0).click();
    assertEquals(1, testProxy.getHandler().getCallCount('onPromoClicked'));
    assertDeepEquals(
        [promo.id], testProxy.getHandler().getArgs('onPromoClicked'));
  });

  test('dismiss promo', async () => {
    individualPromos.onSetPromo(promo);
    await waitForVisibilityEvents();
    const menuButton =
        individualPromos.shadowRoot.querySelector<HTMLElement>('#menuButton');
    assertTrue(!!menuButton);
    menuButton.click();
    await waitForVisibilityEvents();

    const actionMenu = individualPromos.$.actionMenu;
    assertTrue(actionMenu.open);

    const dismissOption =
        actionMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!dismissOption);
    dismissOption.click();

    assertEquals(1, testProxy.getHandler().getCallCount('onPromoDismissed'));
    assertDeepEquals(
        [promo.id], testProxy.getHandler().getArgs('onPromoDismissed'));
    assertFalse(actionMenu.open);
  });
});
