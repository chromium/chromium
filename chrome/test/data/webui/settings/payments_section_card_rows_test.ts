// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CardBenefitsUserAction, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry, STUB_USER_ACCOUNT_INFO} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations, getLocalAndServerCreditCardListItems, getCardRowShadowRoot} from './payments_section_utils.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('PaymentsSectionCardRows', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  interface BenefitsTestCase {
    benefitsAvailable: boolean;
    productTermsUrlAvailable: boolean;
  }

  function cleanUpWhitespace(sublabelElement: HTMLElement) {
    return sublabelElement!.textContent!.trim()
        .replace(/\s+/g, ' ')
        .replace(/\n/g, '');
  }
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
    });
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
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

    // Case 5: a card with CVC
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = 'Visa';
    creditCard.cvc = '111';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Visa ending in 1234, CVC saved',
        menuButton!.getAttribute('title'));
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
    const cardImage = getCardRowShadowRoot(section.$.paymentsList)
                          .querySelector<HTMLImageElement>('#cardImage');
    assertTrue(!!cardImage);
    assertTrue(isVisible(cardImage));
    assertEquals(
        'chrome://theme/IDR_AUTOFILL_CC_GENERIC 1x, chrome://theme/IDR_AUTOFILL_CC_GENERIC@2x 2x',
        cardImage.srcset);
  });

  test('verifyLocalCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = true;
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
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyServerCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // No overflow menu for VCN-ineligible server cards.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
  });

  test('verifyVirtualCardEligibleCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
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
    assertFalse(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyVirtualCardEnrolledCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
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
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertFalse(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyAddVirtualCardClicked', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
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

  // Test to verify the correct sublabel is displayed for virtual card when its
  // FPAN(Real card) has CVC saved.
  test('verifyVirtualCardSummarySublabelWhenFpanHasCvc', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = true;
    creditCard.cvc = '***';
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);

    assertEquals(
        'Virtual card turned on | ' +
            loadTimeData.getString('cvcTagForCreditCardListEntry'),
        getCardRowShadowRoot(section.$.paymentsList)
            .querySelector<HTMLElement>(
                '#summarySublabel')!.textContent!.trim());
  });

  const benefitsStatus: BenefitsTestCase[] = [
    {
      benefitsAvailable: true,
      productTermsUrlAvailable: true,
    },
    {
      benefitsAvailable: true,
      productTermsUrlAvailable: false,
    },
    {
      benefitsAvailable: false,
      productTermsUrlAvailable: true,
    },
  ];

  // Test to verify the existence of the benefits tag based on the existence of
  // the benefits and the product terms URL on a virtual card without a CVC.
  benefitsStatus.forEach(({
                           benefitsAvailable,
                           productTermsUrlAvailable,
                         }) => {
    const benefitsStatus = benefitsAvailable ? 'Available' : 'NotAvailable';
    const termUrlStatus =
        productTermsUrlAvailable ? 'Available' : 'NotAvailable';
    const testName = `VirtualCardSummarySublabel_Benefits${
        benefitsStatus}_ProductTermsUrl${termUrlStatus}`;
    test(testName, async () => {
      loadTimeData.overrideValues({
        autofillCardBenefitsAvailable: benefitsAvailable,
      });
      const creditCard = createCreditCardEntry();
      assertTrue(!!creditCard.metadata);
      creditCard.metadata.isLocal = false;
      creditCard.metadata.isVirtualCardEnrollmentEligible = false;
      creditCard.metadata.isVirtualCardEnrolled = true;
      if (benefitsAvailable && productTermsUrlAvailable) {
        creditCard.productTermsUrl = 'https://google.com/';
      }
      await createPaymentsSection(
          [creditCard], /*ibans=*/[], /*prefValues=*/ {});

      const paymentsList = getLocalAndServerCreditCardListItems();

      assertTrue(!!paymentsList);
      assertEquals(1, paymentsList.length);
      assertTrue(
          isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
              '#summarySublabel')));

      // Build the expected resulting sublabel based on which features are
      // enabled.
      let benefitExpectedSublabel =
          loadTimeData.getString('virtualCardTurnedOn');
      if (benefitsAvailable && productTermsUrlAvailable) {
        benefitExpectedSublabel += ' | ' +
            loadTimeData.getString('benefitsTermsTagForCreditCardListEntry');
      }

      assertEquals(
          benefitExpectedSublabel,
          cleanUpWhitespace(
              paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')!));
      if (benefitsAvailable && productTermsUrlAvailable) {
        const termsLink =
            paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
                '#summaryTermsLink');
        assertTrue(!!termsLink);
        assertEquals(creditCard.productTermsUrl, termsLink.href);
      } else {
        assertFalse(
            isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                '#summaryTermsLink')));
      }
    });
  });

  // Test to verify the existence of the benefits tag based on the existence of
  // the benefits and the product terms URL on a virtual card with a CVC saved.
  benefitsStatus.forEach(({
                           benefitsAvailable,
                           productTermsUrlAvailable,
                         }) => {
    const benefitsStatus = benefitsAvailable ? 'Available' : 'NotAvailable';
    const termUrlStatus =
        productTermsUrlAvailable ? 'Available' : 'NotAvailable';
    const testName = `VirtualCardWithCvcSummarySublabel_Benefits${
        benefitsStatus}_ProductTermsUrl${termUrlStatus}`;
    test(testName, async () => {
      loadTimeData.overrideValues({
        cvcStorageAvailable: true,
        autofillCardBenefitsAvailable: benefitsAvailable,
      });
      const creditCard = createCreditCardEntry();
      assertTrue(!!creditCard.metadata);
      creditCard.metadata.isLocal = false;
      creditCard.metadata.isVirtualCardEnrollmentEligible = false;
      creditCard.metadata.isVirtualCardEnrolled = true;
      creditCard.cvc = '***';
      if (benefitsAvailable && productTermsUrlAvailable) {
        creditCard.productTermsUrl = 'https://google.com/';
      }
      await createPaymentsSection(
          [creditCard], /*ibans=*/[], /*prefValues=*/ {});

      const paymentsList = getLocalAndServerCreditCardListItems();

      assertTrue(!!paymentsList);
      assertEquals(1, paymentsList.length);
      assertTrue(
          isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
              '#summarySublabel')));

      // Build the expected resulting sublabel based on which features are
      // enabled.
      let benefitExpectedSublabel =
          loadTimeData.getString('virtualCardTurnedOn') + ' | ' +
          loadTimeData.getString('cvcTagForCreditCardListEntry');
      if (benefitsAvailable && productTermsUrlAvailable) {
        benefitExpectedSublabel += ' | ' +
            loadTimeData.getString('benefitsTermsTagForCreditCardListEntry');
      }

      assertEquals(
          benefitExpectedSublabel,
          cleanUpWhitespace(
              paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')!));
      if (benefitsAvailable && productTermsUrlAvailable) {
        const termsLink =
            paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
                '#summaryTermsLink');
        assertTrue(!!termsLink);
        assertEquals(creditCard.productTermsUrl, termsLink.href);
      } else {
        assertFalse(
            isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                '#summaryTermsLink')));
      }
    });
  });

  // Test to verify the existence of the benefits tag based on the existence of
  // the benefits and the product terms URL on a server card without a CVC.
  benefitsStatus.forEach(({
                           benefitsAvailable,
                           productTermsUrlAvailable,
                         }) => {
    const benefitsStatus = benefitsAvailable ? 'Available' : 'NotAvailable';
    const termUrlStatus =
        productTermsUrlAvailable ? 'Available' : 'NotAvailable';
    const testName = `ServerCardSummarySublabel_Benefits${
        benefitsStatus}_ProductTermsUrl${termUrlStatus}`;
    test(testName, async () => {
      loadTimeData.overrideValues({
        autofillCardBenefitsAvailable: benefitsAvailable,
      });
      const serverCreditCard = createCreditCardEntry();
      assertTrue(!!serverCreditCard.metadata);
      serverCreditCard.metadata.isLocal = false;
      if (benefitsAvailable && productTermsUrlAvailable) {
        serverCreditCard.productTermsUrl = 'https://google.com/';
      }
      await createPaymentsSection(
          [serverCreditCard], /*ibans=*/[], /*prefValues=*/ {});

      const paymentsList = getLocalAndServerCreditCardListItems();

      assertTrue(!!paymentsList);
      assertEquals(1, paymentsList.length);
      assertTrue(
          isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
              '#summarySublabel')));

      // Build the expected resulting sublabel based on which features are
      // enabled.
      let benefitExpectedSublabel = serverCreditCard.expirationMonth + '/' +
          serverCreditCard.expirationYear!.toString().substring(2);
      if (benefitsAvailable && productTermsUrlAvailable) {
        benefitExpectedSublabel += ' | ' +
            loadTimeData.getString('benefitsTermsTagForCreditCardListEntry');
      }

      assertEquals(
          benefitExpectedSublabel,
          cleanUpWhitespace(
              paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')!));
      if (benefitsAvailable && productTermsUrlAvailable) {
        const termsLink =
            paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
                '#summaryTermsLink');
        assertTrue(!!termsLink);
        assertEquals(serverCreditCard.productTermsUrl, termsLink.href);
      } else {
        assertFalse(
            isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                '#summaryTermsLink')));
      }
    });
  });

  // Test to verify the existence of the benefits tag based on the existence of
  // the benefits and the product terms URL on a server card with a CVC saved.
  benefitsStatus.forEach(({
                           benefitsAvailable,
                           productTermsUrlAvailable,
                         }) => {
    const benefitsStatus = benefitsAvailable ? 'Available' : 'NotAvailable';
    const termUrlStatus =
        productTermsUrlAvailable ? 'Available' : 'NotAvailable';
    const testName = `ServerCardWithCvcSummarySublabel_Benefits${
        benefitsStatus}_ProductTermsUrl${termUrlStatus}`;
    test(testName, async () => {
      loadTimeData.overrideValues({
        cvcStorageAvailable: true,
        autofillCardBenefitsAvailable: benefitsAvailable,
      });
      const serverCreditCard = createCreditCardEntry();
      assertTrue(!!serverCreditCard.metadata);
      serverCreditCard.metadata.isLocal = false;
      serverCreditCard.cvc = '***';
      if (benefitsAvailable && productTermsUrlAvailable) {
        serverCreditCard.productTermsUrl = 'https://google.com/';
      }
      await createPaymentsSection(
          [serverCreditCard], /*ibans=*/[], /*prefValues=*/ {});

      const paymentsList = getLocalAndServerCreditCardListItems();

      assertTrue(!!paymentsList);
      assertEquals(1, paymentsList.length);
      assertTrue(
          isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
              '#summarySublabel')));
      let benefitExpectedSublabel = serverCreditCard.expirationMonth + '/' +
          serverCreditCard.expirationYear!.toString().substring(2) + ' | ' +
          loadTimeData.getString('cvcTagForCreditCardListEntry');
      if (benefitsAvailable && productTermsUrlAvailable) {
        benefitExpectedSublabel += ' | ' +
            loadTimeData.getString('benefitsTermsTagForCreditCardListEntry');
      }
      assertEquals(
          benefitExpectedSublabel,
          cleanUpWhitespace(
              paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                  '#summarySublabel')!));
      if (benefitsAvailable && productTermsUrlAvailable) {
        const termsLink =
            paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
                '#summaryTermsLink');
        assertTrue(!!termsLink);
        assertEquals(serverCreditCard.productTermsUrl, termsLink.href);
      } else {
        assertFalse(
            isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
                '#summaryTermsLink')));
      }
    });
  });

  // Test to verify that clicking the card benefits terms link correctly opens
  // the terms link and records a user action.
  test('verifyCardBenefitsUserActionLoggingOnTermsLinkClick', async function() {
    loadTimeData.overrideValues({
      autofillCardBenefitsAvailable: true,
    });

    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.productTermsUrl = 'https://google.com/';
    await createPaymentsSection([creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const paymentsList = getLocalAndServerCreditCardListItems();

    assertTrue(!!paymentsList);
    assertEquals(1, paymentsList.length);
    assertTrue(
        isVisible(paymentsList[0]!.shadowRoot!.querySelector<HTMLElement>(
            '#summarySublabel')));
    const termsLink =
        paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
            '#summaryTermsLink');
    assertTrue(!!termsLink);
    assertEquals(creditCard.productTermsUrl, termsLink.href);
    // Prevent new tabs from opening, as this will cause the test to run in the
    // background and timeout.
    termsLink.addEventListener('click', function(e) {
      e.preventDefault();
    });
    termsLink.click();

    const userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        CardBenefitsUserAction.CARD_BENEFITS_TERMS_LINK_CLICKED, userAction);
  });

  // Test to verify the benefit terms link has an aria label that includes
  // the card network name and last four digits.
  test('verifyCardBenefitsTermsAriaLabel', async () => {
    loadTimeData.overrideValues({
      autofillCardBenefitsAvailable: true,
    });

    const creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = 'Visa';
    creditCard.metadata!.isLocal = false;
    creditCard.productTermsUrl = 'https://google.com/';
    await createPaymentsSection([creditCard], /*ibans=*/[], /*prefValues=*/ {});

    const paymentsList = getLocalAndServerCreditCardListItems();
    assertTrue(!!paymentsList);
    const termsLink =
        paymentsList[0]!.shadowRoot!.querySelector<HTMLAnchorElement>(
            '#summaryTermsLink');
    assertTrue(!!termsLink);

    const description = loadTimeData.substituteString(
        loadTimeData.getString('creditCardDescription'), creditCard.network,
        creditCard.cardNumber.substring(creditCard.cardNumber.length - 4));
    const expectedAriaLabel = loadTimeData.substituteString(
        loadTimeData.getString('benefitsTermsAriaLabel'), description);
    assertEquals(termsLink.ariaLabel, expectedAriaLabel);
  });

  // Test to verify the cvc tag is visible when cvc is present on a
  // server/local cards.
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
