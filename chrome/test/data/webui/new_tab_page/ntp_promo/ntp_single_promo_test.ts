// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import './ntp_promo_test_common.js';

import type {NtpSinglePromoElement} from 'chrome://new-tab-page/lazy_load.js';
import {getTrustedHTML} from 'chrome://new-tab-page/new_tab_page.js';
import type {Promo} from 'chrome://new-tab-page/ntp_promo.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestNtpPromoProxy} from './ntp_promo_test_common.js';

suite('NtpSinglePromoTest', () => {
  let testProxy: TestNtpPromoProxy;
  let ntpPromo: NtpSinglePromoElement;
  const promo: Promo = {
    id: 'promo',
    iconName: '',
    bodyText: 'body text',
    buttonText: 'button text',
  };
  const promo2: Promo = {
    id: 'promo2',
    iconName: '',
    bodyText: 'body text 2',
    buttonText: 'button text 2',
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
    await waitAfterNextRender(ntpPromo);
    return waitAfterNextRender(ntpPromo);
  }

  setup(() => {
    testProxy = TestNtpPromoProxy.install();

    document.body.innerHTML = getTrustedHTML`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='bodyText'>This is some body text</p>
    </div>`;

    ntpPromo = document.createElement('ntp-single-promo');
    ntpPromo.id = 'ntpPromo';
    document.querySelector<HTMLElement>('#container')!.insertBefore(
        ntpPromo, document.querySelector<HTMLElement>('#bodyText'));

    return waitForVisibilityEvents();
  });

  test('promo starts invisible', () => {
    assertFalse(isVisible(ntpPromo), 'promo should not start visible');
  });

  test('set promo', async () => {
    const promos = [promo];
    ntpPromo.onSetPromos(promos);
    await waitForVisibilityEvents();
    assertTrue(isVisible(ntpPromo), 'promo frame should become visible');
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    assertDeepEquals(
        [[[promo.id], []]], testProxy.getHandler().getArgs('onPromosShown'));
    assertEquals(promo.bodyText, ntpPromo.$.bodyText.innerText);
    assertEquals(promo.buttonText, ntpPromo.$.actionButton.ariaLabel);
  });

  test('set multiple promos chooses the first', async () => {
    const promos = [promo, promo2];
    ntpPromo.onSetPromos(promos);
    await waitForVisibilityEvents();
    assertTrue(isVisible(ntpPromo), 'promo frame should become visible');
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    assertDeepEquals(
        [[[promo.id], []]], testProxy.getHandler().getArgs('onPromosShown'));
    assertEquals(promo.bodyText, ntpPromo.$.bodyText.innerText);
    assertEquals(promo.buttonText, ntpPromo.$.actionButton.ariaLabel);
  });

  test('press button', async () => {
    const promos = [promo];
    ntpPromo.onSetPromos(promos);
    await waitForVisibilityEvents();
    ntpPromo.$.actionButton.click();
    assertEquals(1, testProxy.getHandler().getCallCount('onPromoClicked'));
    assertDeepEquals(
        [promo.id], testProxy.getHandler().getArgs('onPromoClicked'));
  });
});
