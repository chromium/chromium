// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {createCreditCardEntry, STUB_USER_ACCOUNT_INFO, TestPaymentsManager} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations, getLocalAndServerCreditCardListItems, getCardRowShadowRoot} from './payments_section_utils.js';

import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('PaymentsSectionCardRows', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
    });
  });

  test('verifyCreditCardFields', async function() {
    const creditCard = createCreditCardEntry();
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertEquals(
        creditCard.metadata!.summaryLabel,
        rowShadowRoot.querySelector<HTMLElement>(
                         '#summaryLabel')!.textContent!.trim());
  });

  test('verifyCreditCardRowButtonIsDropdownWhenLocal', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertFalse(!!outlinkButton);
  });

  test('verifyCreditCardMoreDetailsTitle', async function() {
    let creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const updateCreditCardCallback =
        (creditCard: chrome.autofillPrivate.CreditCardEntry) => {
          (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
              .lastCallback.setPersonalDataManagerListener!
              ([], [creditCard], [], {
                ...STUB_USER_ACCOUNT_INFO,
                isSyncEnabledForAutofillProfiles: true,
              });
          flush();
        };

    // Case 1: a card with a nickname
    creditCard = createCreditCardEntry();
    creditCard.nickname = 'My card name';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for My card name', menuButton!.getAttribute('title'));

    // Case 2: a card without nickname
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = 'Visa';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Visa ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 3: a card without network
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Card ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 4: a card without number
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Jane Doe', menuButton!.getAttribute('title'));
  });

  test('verifyCreditCardRowButtonIsOutlinkWhenRemote', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });

  test(
      'verifyCreditCardRowButtonIsDropdownWhenVirtualCardEnrollEligible',
      async function() {
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});
        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
        assertTrue(!!menuButton);
        const outlinkButton =
            rowShadowRoot.querySelector('cr-icon-button.icon-external');
        assertFalse(!!outlinkButton);
      });

  test('verifyPaymentsIndicator', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#paymentsIndicator')!.hidden);
  });

  test('verifyCardImage', async function() {
    const creditCard = createCreditCardEntry();
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#cardImage')!.hidden);
  });

  test('verifyLocalCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    // When credit card is local, |isCached| will be undefined.
    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isCached = undefined;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Local credit cards will show the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertFalse(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyCachedCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Cached remote CCs will show overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertFalse(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyNotCachedCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // No overflow menu when not cached.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
  });

  test('verifyClearCachedCreditCardClicked', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuClearCreditCard.hidden);
    section.$.menuClearCreditCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.clearedCachedCreditCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyVirtualCardEligibleCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertFalse(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyVirtualCardEnrolledCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertFalse(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyAddVirtualCardClicked', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuAddVirtualCard.hidden);
    section.$.menuAddVirtualCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.addedVirtualCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveVirtualCardClicked', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuRemoveVirtualCard.hidden);
    section.$.menuRemoveVirtualCard.click();
    flush();

    const menu =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardSharedMenu');
    assertFalse(!!menu);
  });

  test('verifyCreditCardSummarySublabelWithExpirationDate', async function() {
    const creditCard = createCreditCardEntry();

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
    assertTrue(!!creditCard.expirationMonth);
    assertTrue(!!creditCard.expirationYear);
    assertEquals(
        parseInt(creditCard.expirationMonth, 10) + '/' +
            creditCard.expirationYear.substring(2),
        getCardRowShadowRoot(section.$.paymentsList)
            .querySelector<HTMLElement>(
                '#summarySublabel')!.textContent!.trim());
  });

  test('verifyCreditCardSummarySublabelWhenSublabelIsValid', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
  });

  test(
      'verifyCreditCardSummarySublabelWhenVirtualCardAvailable',
      async function() {
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertFalse(
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>('#summarySublabel')!.hidden);
        assertTrue(!!creditCard.expirationMonth);
        assertTrue(!!creditCard.expirationYear);
        assertEquals(
            parseInt(creditCard.expirationMonth, 10) + '/' +
                creditCard.expirationYear.substring(2),
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>(
                    '#summarySublabel')!.textContent!.trim());
      });

  test(
      'verifyCreditCardSummarySublabelWhenVirtualCardTurnedOn',
      async function() {
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
        creditCard.metadata!.isVirtualCardEnrolled = true;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertFalse(
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>('#summarySublabel')!.hidden);
        assertEquals(
            'Virtual card turned on',
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>(
                    '#summarySublabel')!.textContent!.trim());
      });

  // Test to verify the cvc tag is visible when cvc is present on a server/local
  // cards.
  [true, false].forEach(cvcOnServerCard => {
    test(
        'verifyCvcTagPresentFor_' +
            (cvcOnServerCard ? 'ServerCard' : 'LocalCard'),
        async function() {
          loadTimeData.overrideValues({
            cvcStorageAvailable: true,
          });
          const serverCreditCard = createCreditCardEntry();
          serverCreditCard.metadata!.isLocal = false;
          const localCreditCard = createCreditCardEntry();
          if (cvcOnServerCard) {
            serverCreditCard.cvc = '***';
          } else {
            localCreditCard.cvc = '***';
          }
          await createPaymentsSection(
              [serverCreditCard, localCreditCard], /*ibans=*/[],
              /*prefValues=*/ {});

          let serverCardExpectedSublabel = serverCreditCard.expirationMonth +
              '/' + serverCreditCard.expirationYear!.toString().substring(2);
          let localCardExpectedSublabel = localCreditCard.expirationMonth +
              '/' + localCreditCard.expirationYear!.toString().substring(2);
          if (cvcOnServerCard) {
            serverCardExpectedSublabel +=
                ' | ' + loadTimeData.getString('cvcTagForCreditCardListEntry');
          } else {
            localCardExpectedSublabel +=
                ' | ' + loadTimeData.getString('cvcTagForCreditCardListEntry');
          }

          const paymentsList = getLocalAndServerCreditCardListItems();
          assertTrue(!!paymentsList);
          assertEquals(2, paymentsList.length);
          assertTrue(
              isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')));
          assertTrue(
              isVisible(paymentsList[1]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')));
          assertEquals(
              serverCardExpectedSublabel,
              paymentsList[0]!.shadowRoot!
                  .querySelector<HTMLElement>(
                      '#summarySublabel')!.textContent!.trim());
          assertEquals(
              localCardExpectedSublabel,
              paymentsList[1]!.shadowRoot!
                  .querySelector<HTMLElement>(
                      '#summarySublabel')!.textContent!.trim());
        });
  });
});

suite('PaymentsSectionEditCreditCardLink', function() {
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      managePaymentMethodsUrl: 'http://dummy.url/?',
      migrationEnabled: true,
      showIbansSettings: true,
      updateChromeSettingsLinkToGPayWebEnabled: true,
    });
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  test('verifyServerCardLinkToGPayAppendsInstrumentId', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.instrumentId = '123';

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertFalse(!!menuButton);

    const outlinkButton = rowShadowRoot.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
    outlinkButton!.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('managePaymentMethodsUrl') +
            'id=' + creditCard.instrumentId,
        url);
  });

  test(
      'verifyServerCardLinkToGPayDoesNotAppendInstrumentIdIfEmpty',
      async function() {
        const creditCard = createCreditCardEntry();

        creditCard.metadata!.isLocal = false;
        creditCard.instrumentId = '';

        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});

        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
        assertFalse(!!menuButton);

        const outlinkButton = rowShadowRoot.querySelector<HTMLElement>(
            'cr-icon-button.icon-external');
        assertTrue(!!outlinkButton);
        outlinkButton!.click();

        const url = await openWindowProxy.whenCalled('openUrl');
        assertEquals(loadTimeData.getString('managePaymentMethodsUrl'), url);
      });

  test(
      'verifyServerCardLinkToGPayDoesNotAppendInstrumentId_FlagDisabled',
      async function() {
        loadTimeData.overrideValues({
          updateChromeSettingsLinkToGPayWebEnabled: false,
        });
        const creditCard = createCreditCardEntry();

        creditCard.metadata!.isLocal = false;
        creditCard.instrumentId = '123';

        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});

        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
        assertFalse(!!menuButton);

        const outlinkButton = rowShadowRoot.querySelector<HTMLElement>(
            'cr-icon-button.icon-external');
        assertTrue(!!outlinkButton);
        outlinkButton!.click();

        const url = await openWindowProxy.whenCalled('openUrl');
        assertEquals(loadTimeData.getString('managePaymentMethodsUrl'), url);
      });

  test(
      'verifyVirtualCardEligibleLinkToGPayAppendsInstrumentId',
      async function() {
        const creditCard = createCreditCardEntry();

        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        creditCard.instrumentId = '123';

        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*prefValues=*/ {});

        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
        const menuButton =
            rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
        assertTrue(!!menuButton);
        menuButton.click();
        flush();

        assertTrue(isVisible(section.$.menuEditCreditCard));
        section.$.menuEditCreditCard.click();

        const url = await openWindowProxy.whenCalled('openUrl');
        assertEquals(
            loadTimeData.getString('managePaymentMethodsUrl') +
                'id=' + creditCard.instrumentId,
            url);
      });
});
