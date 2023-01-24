// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DiscountConsentCard} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertStyle} from '../../test_support.js';

suite('NewTabPageDiscountConsentCartTest', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      modulesCartConsentStepTwoDifferentColor: false,
      modulesCartDiscountInlineCardShowCloseButton: false,
      modulesCartDiscountConsentVariation: 2,
      modulesCartStepOneUseStaticContent: true,
      modulesCartConsentStepOneButton: 'Continue',
      modulesCartStepOneStaticContent: 'Step one content',
      modulesCartConsentStepTwoContent: 'Step two content',
    });
  });

  let discountConsentCard: DiscountConsentCard;
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    discountConsentCard = document.createElement('discount-consent-card');
    document.body.appendChild(discountConsentCard);

    return flushTasks();
  });

  test('Verify DOM has two steps', async () => {
    const cart = [{
      merchant: 'Amazon',
      cartUrl: {url: 'https://amazon.com'},
      productImageUrls: [
        {url: 'https://image1.com'},
        {url: 'https://image2.com'},
        {url: 'https://image3.com'},
      ],
      discountText: '',
    }];

    discountConsentCard.merchants = cart;
    await flushTasks();

    const contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
        '#contentSteps .step-container');
    assertEquals(contentSteps.length, 2);
    assertEquals(
        'step1', contentSteps[0]!.getAttribute('id'),
        'First content step should have id as step1');
    assertEquals(
        'Step one content',
        contentSteps[0]!.querySelector('.content')!.textContent!.trim());
    assertEquals(
        'step2', contentSteps[1]!.getAttribute('id'),
        'Second content step should have id as step2');
    assertEquals(
        'Step two content',
        contentSteps[1]!.querySelector('.content')!.textContent!.trim());
  });

  test('Verify clicking continue button shows step 2 inline', () => {
    let contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
        '#contentSteps .iron-selected');
    assertEquals(contentSelectedPage.length, 1);
    assertEquals(
        'step1', contentSelectedPage[0]!.getAttribute('id'),
        'Selected content step should have id as step1');

    contentSelectedPage[0]!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
        '#contentSteps .iron-selected');
    assertEquals(contentSelectedPage.length, 1);
    assertEquals(
        'step2', contentSelectedPage[0]!.getAttribute('id'),
        'Selected content step should have id as step2');
  });

  test('Verify clicking continue button logs user action', () => {
    const metrics = fakeMetricsPrivate();
    assertEquals(
        0, metrics.count('NewTabPage.Carts.ShowIntereseInDiscountConsent'),
        'Continue count should be 0 before clicking');
    const contentSelectedPage =
        discountConsentCard.shadowRoot!.querySelectorAll(
            '#contentSteps .iron-selected');
    contentSelectedPage[0]!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    assertEquals(
        1, metrics.count('NewTabPage.Carts.ShowInterestInDiscountConsent'),
        'Continue count should be 1 after clicking');
  });

  test(
      'Verify "Continue" button emits discount-consent-continued event', () => {
        let capturedEvent = false;
        discountConsentCard.addEventListener(
            'discount-consent-continued', () => capturedEvent = true);

        const contentSelectedPage =
            discountConsentCard.shadowRoot!.querySelectorAll(
                '#contentSteps .iron-selected');
        contentSelectedPage[0]!.querySelector<HTMLElement>(
                                   '.action-button')!.click();
        assertTrue(
            capturedEvent, '\'discount-consent-continued\' should be emitted');
      });

  test(
      'Verify "Get discounts" button emits discount-consent-accepted event',
      () => {
        discountConsentCard.currentStep = 1;

        let capturedEvent = false;
        discountConsentCard.addEventListener(
            'discount-consent-accepted', () => capturedEvent = true);

        const contentSelectedPage =
            discountConsentCard.shadowRoot!.querySelectorAll(
                '#contentSteps .iron-selected');
        assertEquals(contentSelectedPage.length, 1);
        assertEquals(
            'step2', contentSelectedPage[0]!.getAttribute('id'),
            'Selected content step should have id as step2');

        contentSelectedPage[0]!.querySelector<HTMLElement>(
                                   '.action-button')!.click();
        assertTrue(
            capturedEvent, '\'discount-consent-accepted\' should be emitted');
      });

  test(
      'Verify "No thanks" button emits discount-consent-rejected event',
      async () => {
        discountConsentCard.currentStep = 1;
        await flushTasks();

        let capturedEvent = false;
        discountConsentCard.addEventListener(
            'discount-consent-rejected', () => capturedEvent = true);

        const contentSelectedPage =
            discountConsentCard.shadowRoot!.querySelectorAll(
                '#contentSteps .iron-selected');
        assertEquals(contentSelectedPage.length, 1);
        assertEquals(
            'step2', contentSelectedPage[0]!.getAttribute('id'),
            'Selected content step should have id as step2');

        contentSelectedPage[0]!.querySelector<HTMLElement>(
                                   '.cancel-button')!.click();
        assertTrue(
            capturedEvent, '"discount-consent-rejected" should be emitted');
      });

  function buildFaviconUrl(merchantUrl: string): string {
    const query = '?size=20&scaleFactor=1x&showFallbackMonogram=&pageUrl=';
    return `chrome://favicon2/${query}${encodeURIComponent(merchantUrl)}`;
  }

  test('Verify favicon is loaded', async () => {
    const carts = [
      {
        merchant: 'Amazon',
        cartUrl: {url: 'https://amazon.com'},
        productImageUrls: [
          {url: 'https://image1.com'},
          {url: 'https://image2.com'},
          {url: 'https://image3.com'},
        ],
        discountText: '',
      },
      {
        merchant: 'eBay',
        cartUrl: {url: 'https://ebay.com'},
        productImageUrls:
            [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
        discountText: '',
      },
      {
        merchant: 'BestBuy',
        cartUrl: {url: 'https://bestbuy.com'},
        productImageUrls: [],
        discountText: '',
      },
    ];

    discountConsentCard.merchants = carts;
    await flushTasks();

    const favicons = discountConsentCard.shadowRoot!.querySelectorAll(
        '#faviconContainer ul.favicon-list li.favicon');
    assertEquals(3, favicons.length, 'There should be three favicons.');

    assertEquals(
        buildFaviconUrl(carts[0]!.cartUrl.url),
        favicons[0]!.querySelector('.favicon-image')!.getAttribute('src'));
    assertEquals(
        buildFaviconUrl(carts[1]!.cartUrl.url),
        favicons[1]!.querySelector('.favicon-image')!.getAttribute('src'));
    assertEquals(
        buildFaviconUrl(carts[2]!.cartUrl.url),
        favicons[2]!.querySelector('.favicon-image')!.getAttribute('src'));
  });

  test('Verify close button is hidden', async () => {
    discountConsentCard.currentStep = 1;
    await flushTasks();

    const closeButton = discountConsentCard.shadowRoot!.querySelector('#close');
    assertTrue(closeButton === null, 'closeButton should not exist in the dom');
  });

  test(
      'Verify step 2 has two buttons when close button is hidden', async () => {
        discountConsentCard.currentStep = 1;
        await flushTasks();

        const contentSelectedPage =
            discountConsentCard.shadowRoot!.querySelectorAll(
                '#contentSteps .iron-selected');
        const buttons = contentSelectedPage[0]!.querySelectorAll(
            '.button-container cr-button');
        assertEquals(2, buttons.length, 'This step should have two buttons');
      });

  suite('Show Close button', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues(
          {modulesCartDiscountInlineCardShowCloseButton: true});
    });

    test('Verify close button is shown', async () => {
      await flushTasks();

      const closeButton =
          discountConsentCard.shadowRoot!.querySelector('#close');
      assertTrue(closeButton !== null, 'closeButton should exist in the DOM');
    });

    test('Verify step 2 has one button', async () => {
      discountConsentCard.currentStep = 1;
      await flushTasks();

      const contentSelectedPage =
          discountConsentCard.shadowRoot!.querySelectorAll(
              '#contentSteps .iron-selected');
      const buttons = contentSelectedPage[0]!.querySelectorAll(
          '.button-container cr-button');
      assertEquals(1, buttons.length, 'This step should have one button');
      assertTrue(true);
    });

    test(
        'Verify clicking close button in step 1 emits dismissed event',
        async () => {
          discountConsentCard.currentStep = 0;
          await flushTasks();

          let capturedEvent = false;
          discountConsentCard.addEventListener(
              'discount-consent-dismissed', () => capturedEvent = true);

          discountConsentCard.shadowRoot!.querySelector<HTMLElement>(
                                             '#close')!.click();
          assertTrue(
              capturedEvent,
              '\'discount-consent-dismissed\' should be emitted');
        });

    test(
        'Verify clicking close button in step 2 emits rejected event',
        async () => {
          discountConsentCard.currentStep = 1;
          await flushTasks();

          let capturedEvent = false;
          discountConsentCard.addEventListener(
              'discount-consent-rejected', () => capturedEvent = true);

          discountConsentCard.shadowRoot!.querySelector<HTMLElement>(
                                             '#close')!.click();
          assertTrue(
              capturedEvent, '\'discount-consent-rejected\' should be emitted');
        });
  });

  suite('Step two background has different color', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesCartConsentStepTwoDifferentColor: true,
        modulesCartDiscountInlineCardShowCloseButton: false,
      });
    });

    test('Verify step 2 has background color', () => {
      discountConsentCard.currentStep = 1;
      const consentCardContainer =
          discountConsentCard.shadowRoot!.querySelector(
              '#consentCardContainer');
      const goolgeBlue100 = 'rgb(210, 227, 252)';
      assertStyle(consentCardContainer!, 'background-color', goolgeBlue100);
    });
  });

  suite('Static content disabled for step one of cart module', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesCartStepOneUseStaticContent: false,
        modulesCartConsentStepOneOneMerchantContent: 'One merchant: $1',
        modulesCartConsentStepOneTwoMerchantsContent:
            'Two merchants: $1 and $2',
        modulesCartConsentStepOneThreeMerchantsContent:
            'Three merchants: $1, $2, and more',
      });
    });

    test('Verify step one content when one merchant cart shown', async () => {
      const cart = [{
        merchant: 'Amazon',
        cartUrl: {url: 'https://amazon.com'},
        productImageUrls: [
          {url: 'https://image1.com'},
          {url: 'https://image2.com'},
          {url: 'https://image3.com'},
        ],
        discountText: '',
      }];

      discountConsentCard.merchants = cart;
      await flushTasks();

      const contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
          '#contentSteps .step-container');

      assertEquals(
          'step1', contentSteps[0]!.getAttribute('id'),
          'First content step should have id as step1');
      assertEquals(
          'One merchant: Amazon',
          contentSteps[0]!.querySelector('.content')!.textContent!.trim());
    });

    test('Verify step one content when two merchant carts shown', async () => {
      const carts = [
        {
          merchant: 'Amazon',
          cartUrl: {url: 'https://amazon.com'},
          productImageUrls: [
            {url: 'https://image1.com'},
            {url: 'https://image2.com'},
            {url: 'https://image3.com'},
          ],
          discountText: '',
        },
        {
          merchant: 'eBay',
          cartUrl: {url: 'https://ebay.com'},
          productImageUrls:
              [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
          discountText: '',
        },
      ];

      discountConsentCard.merchants = carts;
      await flushTasks();

      const contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
          '#contentSteps .step-container');

      assertEquals(
          'step1', contentSteps[0]!.getAttribute('id'),
          'First content step should have id as step1');
      assertEquals(
          'Two merchants: Amazon and eBay',
          contentSteps[0]!.querySelector('.content')!.textContent!.trim());
    });

    test(
        'Verify step one content when three or more merchant carts shown',
        async () => {
          const carts = [
            {
              merchant: 'Amazon',
              cartUrl: {url: 'https://amazon.com'},
              productImageUrls: [
                {url: 'https://image1.com'},
                {url: 'https://image2.com'},
                {url: 'https://image3.com'},
              ],
              discountText: '',
            },
            {
              merchant: 'eBay',
              cartUrl: {url: 'https://ebay.com'},
              productImageUrls:
                  [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
              discountText: '',
            },
            {
              merchant: 'BestBuy',
              cartUrl: {url: 'https://bestbuy.com'},
              productImageUrls: [],
              discountText: '',
            },
          ];

          discountConsentCard.merchants = carts;
          await flushTasks();

          const contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
              '#contentSteps .step-container');

          assertEquals(
              'step1', contentSteps[0]!.getAttribute('id'),
              'First content step should have id as step1');
          assertEquals(
              'Three merchants: Amazon, eBay, and more',
              contentSteps[0]!.querySelector('.content')!.textContent!.trim());
        });

    test(
        'Verify step one content updated when merchant cart changed',
        async () => {
          const carts = [{
            merchant: 'Amazon',
            cartUrl: {url: 'https://amazon.com'},
            productImageUrls: [
              {url: 'https://image1.com'},
              {url: 'https://image2.com'},
              {url: 'https://image3.com'},
            ],
            discountText: '',
          }];

          discountConsentCard.merchants = carts;
          await flushTasks();

          let contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
              '#contentSteps .step-container');

          assertEquals(
              'step1', contentSteps[0]!.getAttribute('id'),
              'First content step should have id as step1');
          assertEquals(
              'One merchant: Amazon',
              contentSteps[0]!.querySelector('.content')!.textContent!.trim());

          discountConsentCard.merchants = [
            {
              merchant: 'Amazon',
              cartUrl: {url: 'https://amazon.com'},
              productImageUrls: [
                {url: 'https://image1.com'},
                {url: 'https://image2.com'},
                {url: 'https://image3.com'},
              ],
              discountText: '',
            },
            {
              merchant: 'eBay',
              cartUrl: {url: 'https://ebay.com'},
              productImageUrls:
                  [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
              discountText: '',
            },
          ];

          await flushTasks();

          contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
              '#contentSteps .step-container');

          assertEquals(
              'step1', contentSteps[0]!.getAttribute('id'),
              'First content step should have id as step1');
          assertEquals(
              'Two merchants: Amazon and eBay',
              contentSteps[0]!.querySelector('.content')!.textContent!.trim());
        });
  });

  suite('Enable the Dialog Variation', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesCartDiscountConsentVariation: 3,
        modulesCartSentence: 'Dialog title',
      });
    });

    test('Verify DOM has one step', async () => {
      const cart = [{
        merchant: 'Amazon',
        cartUrl: {url: 'https://amazon.com'},
        productImageUrls: [
          {url: 'https://image1.com'},
          {url: 'https://image2.com'},
          {url: 'https://image3.com'},
        ],
        discountText: '',
      }];

      discountConsentCard.merchants = cart;
      await flushTasks();

      const contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
          '#contentSteps .step-container');
      assertEquals(contentSteps.length, 1);

      assertEquals(
          'step1', contentSteps[0]!.getAttribute('id'),
          'First content step should have id as step1');
    });

    test(
        'Verify clicking continue in one step shows DiscountConsentDialog',
        async () => {
          assertEquals(
              0,
              discountConsentCard.shadowRoot!
                  .querySelectorAll('#discountConsentDialog')
                  .length);

          const contentSelectedPage =
              discountConsentCard.shadowRoot!.querySelectorAll(
                  '#contentSteps .iron-selected');
          assertEquals(contentSelectedPage.length, 1);
          assertEquals(
              'step1', contentSelectedPage[0]!.getAttribute('id'),
              'Selected content step should have id as step1');

          contentSelectedPage[0]!.querySelector<HTMLElement>(
                                     '.action-button')!.click();
          await flushTasks();
          assertEquals(
              1,
              discountConsentCard.shadowRoot!
                  .querySelectorAll('#discountConsentDialog')
                  .length);
        });
  });
});
