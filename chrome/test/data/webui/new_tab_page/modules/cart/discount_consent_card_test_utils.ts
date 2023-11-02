// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiscountConsentCard} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

export function clickAcceptButton(discountConsentCard: DiscountConsentCard) {
  const contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
      '#contentSteps .iron-selected');
  assertEquals(contentSelectedPage.length, 1);
  assertEquals(
      'step2', contentSelectedPage[0]!.getAttribute('id'),
      'Selected content step should have id as step2');

  contentSelectedPage[0]!.querySelector<HTMLElement>('.action-button')!.click();
}

export function clickCloseButton(discountConsentCard: DiscountConsentCard) {
  discountConsentCard.shadowRoot!.querySelector<HTMLElement>('#close')!.click();
}

export function clickRejectButton(discountConsentCard: DiscountConsentCard) {
  const contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
      '#contentSteps .iron-selected');
  assertEquals(contentSelectedPage.length, 1);
  assertEquals(
      'step2', contentSelectedPage[0]!.getAttribute('id'),
      'Selected content step should have id as step2');

  contentSelectedPage[0]!.querySelector<HTMLElement>('.cancel-button')!.click();
}

export function nextStep(discountConsentCard: DiscountConsentCard) {
  assertEquals(
      0, discountConsentCard.currentStep,
      'discountConsentCard is not in step 1');
  const contentSelectedPage = discountConsentCard.shadowRoot!.querySelectorAll(
      '#contentSteps .iron-selected');

  contentSelectedPage[0]!.querySelector<HTMLElement>('.action-button')!.click();
}
