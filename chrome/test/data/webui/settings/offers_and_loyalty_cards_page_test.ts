// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsLoyaltyCardsListElement, SettingsOffersAndLoyaltyCardsPageElement, SettingsOffersListElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createLoyaltyCardEntry, createWalletDirectOfferEntry, TestPaymentsManager} from './autofill_fake_data.js';

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

suite('OffersList', function() {
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
    await paymentsManager.whenCalled('getWalletDirectOfferList');
    await microtasksFinished();
    return page;
  }

  function getList(page: SettingsOffersAndLoyaltyCardsPageElement):
      SettingsOffersListElement {
    const list = page.shadowRoot.querySelector('settings-offers-list');
    assertTrue(!!list);
    return list;
  }

  function getRows(list: SettingsOffersListElement): HTMLElement[] {
    return Array.from(list.shadowRoot.querySelectorAll<HTMLElement>('.offer'));
  }

  function getText(row: HTMLElement, selector: string): string {
    return row.querySelector(selector)!.textContent.trim();
  }

  test('ShowsHeader', async function() {
    const list = getList(await setupPage());
    const header = list.shadowRoot.querySelector('#offersHeader h2');
    assertTrue(!!header);
    assertEquals(
        loadTimeData.getString('offersTitle'), header.textContent.trim());
  });

  test('ShowsOffersCardAboveLoyaltyCardsCard', async function() {
    const page = await setupPage();
    const lists = page.shadowRoot.querySelectorAll(
        'settings-offers-list, settings-loyalty-cards-list');
    assertEquals(2, lists.length);
    assertEquals('settings-offers-list', lists[0]!.localName);
    assertEquals('settings-loyalty-cards-list', lists[1]!.localName);
  });

  test('ShowsEmptyStateWithNoOffers', async function() {
    const list = getList(await setupPage());
    assertEquals(0, getRows(list).length);
    assertTrue(isVisible(list.shadowRoot.querySelector('#offersNone')));
  });

  test('ShowsOffersInGivenOrder', async function() {
    paymentsManager.data.walletDirectOffers = [
      createWalletDirectOfferEntry({
        offerId: 'newer',
        issuerName: 'Zappos',
        shortTitle: '$10 off shoes',
        promoCode: 'SHOES10',
        expirationDate: 'Dec 1, 2026',
      }),
      createWalletDirectOfferEntry(),
    ];
    const list = getList(await setupPage());

    const rows = getRows(list);
    assertEquals(2, rows.length);
    assertFalse(isVisible(list.shadowRoot.querySelector('#offersNone')));

    // Not re-sorted: the most recently issued offer comes first.
    assertEquals('Zappos', getText(rows[0]!, '.issuer-name'));
    assertEquals('$10 off shoes', getText(rows[0]!, '.short-title'));
    assertEquals(
        loadTimeData.getStringF('offersPromoCodeLabel', 'SHOES10'),
        getText(rows[0]!, '.promo-code'));
    assertEquals(
        loadTimeData.getStringF('offersExpirationLabel', 'Dec 1, 2026'),
        getText(rows[0]!, '.expiration'));
    assertTrue(!!rows[0]!.querySelector('.code-expiration-separator'));
    assertTrue(!!rows[0]!.querySelector('.logo cr-icon'));
    assertTrue(!!rows[0]!.querySelector('.wallet-indicator'));

    assertEquals('Sephora', getText(rows[1]!, '.issuer-name'));
    assertEquals(
        '15% off your next purchase', getText(rows[1]!, '.short-title'));
    assertEquals(
        loadTimeData.getStringF('offersPromoCodeLabel', 'WELCOME15'),
        getText(rows[1]!, '.promo-code'));
    assertEquals(
        loadTimeData.getStringF('offersExpirationLabel', 'Nov 20, 2026'),
        getText(rows[1]!, '.expiration'));
  });

  test('WalletLinkOpensOfferDetailsUrl', async function() {
    paymentsManager.data.walletDirectOffers = [
      createWalletDirectOfferEntry(),
      createWalletDirectOfferEntry({
        offerId: 'offer_id_2',
        offerDetailsUrl: 'https://wallet.google.com/second',
      }),
    ];
    const list = getList(await setupPage());

    getRows(list)[1]!.querySelector<HTMLElement>('.wallet-link')!.click();
    assertEquals(
        'https://wallet.google.com/second',
        await openWindowProxy.whenCalled('openUrl'));
  });

  test('UpdatesWhenPersonalDataChanges', async function() {
    const list = getList(await setupPage());
    assertEquals(0, getRows(list).length);

    paymentsManager.data.walletDirectOffers = [createWalletDirectOfferEntry()];
    paymentsManager.resetResolver('getWalletDirectOfferList');
    paymentsManager.lastCallback.setPersonalDataManagerListener!
        ([], [], [], []);
    await paymentsManager.whenCalled('getWalletDirectOfferList');
    await microtasksFinished();

    assertEquals(1, getRows(list).length);
    assertFalse(isVisible(list.shadowRoot.querySelector('#offersNone')));
  });

  test('RemovesListenerWhenDetached', async function() {
    const page = await setupPage();
    assertEquals(
        1, paymentsManager.getCallCount('setPersonalDataManagerListener'));

    page.remove();

    assertEquals(
        1, paymentsManager.getCallCount('removePersonalDataManagerListener'));
  });
});
