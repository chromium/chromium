// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CLICK_NEXT_BUTTON} from 'chrome://shimless-rma/events.js';
import {fakeRsuChallengeQrCode} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingEnterRsuWpDisableCodePage} from 'chrome://shimless-rma/onboarding_enter_rsu_wp_disable_code_page.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('onboardingEnterRsuWpDisableCodePageTest', function() {
  let component: OnboardingEnterRsuWpDisableCodePage|null = null;

  let service: FakeShimlessRmaService|null = null;

  const rsuCodeInputSelector = '#rsuCode';
  const dialogLinkSelector = '#rsuCodeDialogLink';
  const challengeDialogSelector = '#rsuChallengeDialog';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    service = null;
  });

  function initializeEnterRsuWpDisableCodePage(): Promise<void> {
    // Initialize the fake data.
    assert(service);
    service.setGetRsuDisableWriteProtectChallengeResult(
        /* challenge= */ 'rsu challenge');
    service.setGetRsuDisableWriteProtectHwidResult(/* hwid= */ 'hwid');
    service.setGetRsuDisableWriteProtectChallengeQrCodeResponse(
        fakeRsuChallengeQrCode);

    assert(!component);
    component = document.createElement(OnboardingEnterRsuWpDisableCodePage.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  // Verify the page is initialized with the input visible and QR code rendered.
  test('PageInitializes', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    assertFalse(
        strictQuery(rsuCodeInputSelector, component.shadowRoot, CrInputElement)
            .hidden);
    const expectedImgUrlPrefix = 'blob:chrome://shimless-rma/';
    assertTrue(strictQuery('#qrCodeImg', component.shadowRoot, HTMLImageElement)
                   .src.startsWith(expectedImgUrlPrefix));
  });

  // Verify the correct RSU code is sent when the next button is clicked.
  test('SendRsuCode', async () => {
    await initializeEnterRsuWpDisableCodePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let savedCode = '';
    assert(service);
    service.setRsuDisableWriteProtectCode = (code: string) => {
      savedCode = code;
      return resolver.promise;
    };

    assert(component);
    const rsuCodeComponent =
        strictQuery(rsuCodeInputSelector, component.shadowRoot, CrInputElement);
    // Set the code to lower case then expect the input to programmatically
    // change the case back to upper when sent to the service.
    const expectedCode = 'RSU CODE';
    rsuCodeComponent.value = expectedCode.toLowerCase();

    component.onNextButtonClick();
    await resolver;
    assertEquals(expectedCode, savedCode);
  });

  // Verify the dialog is opened when challenge link is clicked.
  test('OpenChallengeDialog', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    strictQuery(dialogLinkSelector, component.shadowRoot, HTMLAnchorElement)
        .click();
    assertTrue(
        strictQuery(
            challengeDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
  });

  // Verify the input is disabled when `allButtonsDisabled` is set.
  test('DisableInput', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    const rsuCodeInput =
        strictQuery(rsuCodeInputSelector, component.shadowRoot, CrInputElement);
    assertFalse(rsuCodeInput.disabled);
    component.allButtonsDisabled = true;
    assertTrue(rsuCodeInput.disabled);
  });

  // Verify the dialog can't be open when `allButtonsDisabled` is set.
  test('StopChallengeDialogOpening', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    component.allButtonsDisabled = true;
    strictQuery(dialogLinkSelector, component.shadowRoot, HTMLAnchorElement)
        .click();
    assertFalse(
        strictQuery(
            challengeDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
  });

  // Verify the code is rejected if it's too short.
  test('RejectWrongCodeLength', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    const rsuCodeInput =
        strictQuery(rsuCodeInputSelector, component.shadowRoot, CrInputElement);

    // The code is shorter than the expected 8 characters.
    rsuCodeInput.value = '12345';
    assertFalse(rsuCodeInput.invalid);

    let rejectedPromise = null;
    try {
      await component.onNextButtonClick();
    } catch (error: unknown) {
      assertTrue(error instanceof Error);
      // Promise was rejected.
      rejectedPromise = error;
    }
    assert(rejectedPromise);
    assertTrue(rsuCodeInput.invalid);

    // Change the code so the invalid goes away.
    rsuCodeInput.value = '123';
    assertFalse(rsuCodeInput.invalid);
  });

  // Verify the code can be submitted with the enter key.
  test('EnterKeySubmitsCode', async () => {
    await initializeEnterRsuWpDisableCodePage();

    assert(component);
    const nextButtonEvent = eventToPromise(CLICK_NEXT_BUTTON, component);

    const rsuCodeInput =
        strictQuery(rsuCodeInputSelector, component.shadowRoot, CrInputElement);
    rsuCodeInput.value = '12345678';
    rsuCodeInput.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await nextButtonEvent;
  });
});
