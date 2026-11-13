// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsLoyaltyCardsListElement, SettingsOffersAndLoyaltyCardsPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPaymentsManager} from './autofill_fake_data.js';

type LoyaltyCardEntry = chrome.autofillPrivate.LoyaltyCardEntry;

function createLoyaltyCardEntry(overrides: Partial<LoyaltyCardEntry> = {}):
    LoyaltyCardEntry {
  return {
    id: 'loyalty_card_id',
    merchantName: 'Merchant',
    loyaltyCardNumber: '1234',
    walletUrl: 'https://wallet.google.com/wallet?p=loyalty',
    ...overrides,
  };
}

suite('OffersAndLoyaltyCardsPage', function() {
  let paymentsManager: TestPaymentsManager;
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function setupPage():
      Promise<SettingsOffersAndLoyaltyCardsPageElement> {
    const page =
        document.createElement('settings-offers-and-loyalty-cards-page');
    document.body.appendChild(page);
    await paymentsManager.whenCalled('getLoyaltyCardList');
    await microtasksFinished();
    return page;
  }

  function getList(page: SettingsOffersAndLoyaltyCardsPageElement):
      SettingsLoyaltyCardsListElement {
    const list = page.shadowRoot.querySelector('settings-loyalty-cards-list');
    assertTrue(!!list);
    return list;
  }

  function getRows(list: SettingsLoyaltyCardsListElement): HTMLElement[] {
    return Array.from(
        list.shadowRoot.querySelectorAll<HTMLElement>('.loyalty-card'));
  }

  test('ShowsHeader', async function() {
    const list = getList(await setupPage());
    const header = list.shadowRoot.querySelector('#loyaltyCardsHeader h2');
    assertTrue(!!header);
    assertEquals(
        loadTimeData.getString('loyaltyCardsTitle'), header.textContent.trim());
  });

  test('ShowsEmptyStateWithNoCards', async function() {
    const list = getList(await setupPage());
    assertEquals(0, getRows(list).length);
    assertTrue(isVisible(list.shadowRoot.querySelector('#loyaltyCardsNone')));
  });

  test('ShowsLoyaltyCards', async function() {
    paymentsManager.data.loyaltyCards = [
      createLoyaltyCardEntry({
        id: 'loyalty_card_id_2',
        merchantName: 'Lidl',
        loyaltyCardNumber: '4321',
      }),
      createLoyaltyCardEntry({merchantName: 'ikea', loyaltyCardNumber: '42'}),
    ];
    const list = getList(await setupPage());

    const rows = getRows(list);
    assertEquals(2, rows.length);
    assertFalse(isVisible(list.shadowRoot.querySelector('#loyaltyCardsNone')));

    // Sorted by merchant name, ignoring case.
    assertEquals(
        'ikea', rows[0]!.querySelector('.merchant-name')!.textContent.trim());
    assertEquals(
        '42', rows[0]!.querySelector('.card-number')!.textContent.trim());
    assertTrue(!!rows[0]!.querySelector('.logo cr-icon'));
    assertTrue(!!rows[0]!.querySelector('.wallet-indicator'));

    assertEquals(
        'Lidl', rows[1]!.querySelector('.merchant-name')!.textContent.trim());
    assertEquals(
        '4321', rows[1]!.querySelector('.card-number')!.textContent.trim());
  });

  test('WalletLinkOpensWalletUrl', async function() {
    paymentsManager.data.loyaltyCards = [
      createLoyaltyCardEntry(),
      createLoyaltyCardEntry({
        id: 'loyalty_card_id_2',
        walletUrl: 'https://wallet.google.com/second',
      }),
    ];
    const list = getList(await setupPage());

    getRows(list)[1]!.querySelector<HTMLElement>('.wallet-link')!.click();
    assertEquals(
        'https://wallet.google.com/second',
        await openWindowProxy.whenCalled('openUrl'));
  });

  test('UpdatesWhenLoyaltyCardsChange', async function() {
    const list = getList(await setupPage());
    assertEquals(0, getRows(list).length);

    paymentsManager.lastCallback.setLoyaltyCardsChangedListener!
        ([createLoyaltyCardEntry()]);
    await microtasksFinished();

    assertEquals(1, getRows(list).length);
    assertFalse(isVisible(list.shadowRoot.querySelector('#loyaltyCardsNone')));
  });

  test('RemovesListenerWhenDetached', async function() {
    const page = await setupPage();
    assertEquals(
        1, paymentsManager.getCallCount('setLoyaltyCardsChangedListener'));

    page.remove();

    assertEquals(
        1, paymentsManager.getCallCount('removeLoyaltyCardsChangedListener'));
  });
});
