// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://new-tab-page/new_tab_page.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DiscountConsentCard} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';
import {assertStyle} from '../../test_support.js';

suite('NewTabPageDiscountConsentCartTest', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues(
        {modulesCartConsentStepTwoDifferentColor: false});
  });

  let discountConsentCard: DiscountConsentCard;
  setup(() => {
    document.body.innerHTML = '';
    discountConsentCard = document.createElement('discount-consent-card');
    document.body.appendChild(discountConsentCard);

    return flushTasks();
  });

  test('Verify DOM has two steps', () => {
    var contentSteps = discountConsentCard.shadowRoot!.querySelectorAll(
        '#contentSteps .step-container');
    assertEquals(contentSteps.length, 2);
    assertEquals(
        'step1', contentSteps[0]!.getAttribute('id'),
        'First content step should have id as step1');
    assertEquals(
        'step2', contentSteps[1]!.getAttribute('id'),
        'Second content step should have id as step2');
  });

  test('Verify clicking continue button shows step 2 inline', () => {
    var contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
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

  test(
      'Verify "Get discounts" button emits discount-consent-accepted event',
      () => {
        discountConsentCard.currentStep = 1;

        var capturedEvent = false;
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
      'Verify "No thanks" button emits discount-consent-rejected event', () => {
        discountConsentCard.currentStep = 1;

        var capturedEvent = false;
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
            capturedEvent, '\'discount-consent-rejected\' should be emitted');
      });

  function buildFaviconUrl(merchantUrl: string): string {
    return 'chrome://favicon2/?size=24&scale_factor=1x&show_fallback_monogram=&page_url=' +
        encodeURIComponent(merchantUrl);
  }

  test('Verify favicon is loaded', async () => {
    const carts = [
      {
        merchant: 'Amazon',
        cartUrl: {url: 'https://amazon.com'},
        productImageUrls: [
          {url: 'https://image1.com'}, {url: 'https://image2.com'},
          {url: 'https://image3.com'}
        ],
        discountText: ''
      },
      {
        merchant: 'eBay',
        cartUrl: {url: 'https://ebay.com'},
        productImageUrls:
            [{url: 'https://image4.com'}, {url: 'https://image5.com'}],
        discountText: ''
      },
      {
        merchant: 'BestBuy',
        cartUrl: {url: 'https://bestbuy.com'},
        productImageUrls: [],
        discountText: ''
      }
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

  suite('Step two background has different color', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues(
          {modulesCartConsentStepTwoDifferentColor: true});
    });

    test('Verfiy step 2 has background color', () => {
      discountConsentCard.currentStep = 1;
      const consentCardContainer =
          discountConsentCard.shadowRoot!.querySelector(
              '#consentCardContainer');
      const goolgeBlue100 = 'rgb(210, 227, 252)';
      assertStyle(consentCardContainer!, 'background-color', goolgeBlue100);
    });
  });
});
