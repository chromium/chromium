// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import './ntp_promo_test_common.js';

import type {IndividualPromosElement} from 'chrome://new-tab-page/lazy_load.js';
import {getTrustedHTML} from 'chrome://new-tab-page/new_tab_page.js';
import type {Promo} from 'chrome://new-tab-page/ntp_promo.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestNtpPromoProxy} from './ntp_promo_test_common.js';

suite('IndividualPromosTest', () => {
  let testProxy: TestNtpPromoProxy;
  let individualPromos: IndividualPromosElement;

  const promos: Promo[] = [];
  const NUM_TEST_PROMOS = 3;
  for (let i = 0; i < NUM_TEST_PROMOS; ++i) {
    promos.push({
      id: 'promo' + i,
      iconName: '',
      bodyText: 'body text ' + i,
      buttonText: 'button text ' + i,
    });
  }

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
    return getContainer().childElementCount;
  }

  function getPromoAt(index: number): HTMLElement {
    return getContainer().children[index] as HTMLElement;
  }

  function ids(start: number, length: number): string[] {
    return promos.slice(start, start + length).map(promo => promo.id);
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
    individualPromos.maxPromos = 1;
    document.querySelector<HTMLElement>('#container')!.insertBefore(
        individualPromos, document.querySelector<HTMLElement>('#bodyText'));

    return waitForVisibilityEvents();
  });

  test('promo starts invisible', () => {
    assertFalse(isVisible(individualPromos), 'promo should not start visible');
  });

  test('set no promos hides promo container', async () => {
    individualPromos.onSetPromos([]);
    await waitForVisibilityEvents();
    assertFalse(isVisible(individualPromos));
  });


  // Check that the default number of showing promos.
  test('show single promo', async () => {
    assertTrue(individualPromos.maxPromos < promos.length);
    individualPromos.onSetPromos(promos);
    await waitForVisibilityEvents();
    assertTrue(
        isVisible(individualPromos), 'promo frame should become visible');
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    assertDeepEquals(
        [[ids(0, 1), []]], testProxy.getHandler().getArgs('onPromosShown'));
    assertEquals(1, getPromoCount());
  });

  // Overriding the number of showable promos should be respected.
  test('show two promos', async () => {
    individualPromos.maxPromos = 2;
    individualPromos.onSetPromos(promos);
    await waitForVisibilityEvents();
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    assertDeepEquals(
        [[ids(0, 2), []]], testProxy.getHandler().getArgs('onPromosShown'));
    assertEquals(2, getPromoCount());
    // Check order.
    for (let i = 0; i < 2; ++i) {
      assertEquals(
          getPromoAt(i).querySelector<HTMLElement>('#bodyText')!.innerText,
          promos[i]!.bodyText);
    }
  });

  test('fewer promos available', async () => {
    individualPromos.maxPromos = 2;
    individualPromos.onSetPromos(promos.slice(0, 1));
    await waitForVisibilityEvents();
    assertEquals(1, getPromoCount());
  });

  test('promo details', async () => {
    individualPromos.onSetPromos(promos);
    await waitForVisibilityEvents();
    const expected = promos[0]!;
    const promo = getPromoAt(0);
    assertEquals(
        promo.querySelector<HTMLElement>('#bodyText')!.innerText,
        expected.bodyText);
    assertEquals(promo.ariaLabel, expected.buttonText);
  });

  test('press button', async () => {
    individualPromos.onSetPromos(promos);
    await waitForVisibilityEvents();
    getPromoAt(0).click();
    assertEquals(1, testProxy.getHandler().getCallCount('onPromoClicked'));
    assertDeepEquals(
        [promos[0]!.id], testProxy.getHandler().getArgs('onPromoClicked'));
  });
});
