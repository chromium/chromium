// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from 'chrome://new-tab-page/lazy_load.js';
import type {ComposeboxSubmitElement} from 'chrome://resources/cr_components/composebox/composebox_submit.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createComposeboxElement, setupComposeboxTest} from './test_support.js';

suite('ComposeboxSubmitTest', () => {
  let submitButton: ComposeboxSubmitElement;

  setup(async () => {
    loadTimeData.overrideValues({
      'composeboxSubmitButtonTitle': 'Submit',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    submitButton = document.createElement('cr-composebox-submit');
    document.body.appendChild(submitButton);
    await microtasksFinished();
  });

  test('renders default properties', () => {
    assertFalse(submitButton.disabled);
    assertEquals(SubmitButtonIconType.UPWARD, submitButton.iconType);

    const icon =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitIcon');
    const overlay =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitOverlay');

    // Assert icon properties.
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('icon-arrow-upward'));
    assertFalse(icon.hasAttribute('disabled'));
    // Assert overlay properties.
    assertTrue(!!overlay);
    assertEquals('Submit', overlay.getAttribute('title'));
  });

  test('renders passed in properties', async () => {
    submitButton.disabled = true;
    submitButton.iconType = SubmitButtonIconType.FORWARD;
    submitButton.submitButtonTitle = 'Custom Title';
    await microtasksFinished();

    const icon =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitIcon');
    const overlay =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitOverlay');

    // Assert icon properties.
    assertTrue(!!icon);
    assertTrue(icon.classList.contains('icon-arrow-forward'));
    assertTrue(icon.hasAttribute('disabled'));
    // Assert overlay properties.
    assertTrue(!!overlay);
    assertEquals('Custom Title', overlay.getAttribute('title'));
  });

  test('dispatches submit-click when not disabled', async () => {
    let eventCount = 0;
    submitButton.addEventListener('submit-click', () => {
      eventCount++;
    });
    const container =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitContainer');
    assertTrue(!!container);

    container.click();
    await microtasksFinished();

    assertEquals(1, eventCount);
  });

  test('does not dispatch submit-click when disabled', async () => {
    let eventCount = 0;
    submitButton.addEventListener('submit-click', () => {
      eventCount++;
    });
    submitButton.disabled = true;
    await microtasksFinished();
    const container =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitContainer');
    assertTrue(!!container);

    container.click();
    await microtasksFinished();

    assertEquals(0, eventCount);
  });

  test('dispatches submit-focusin event', async () => {
    const focusinPromise = eventToPromise('submit-focusin', submitButton);
    const container =
        submitButton.shadowRoot.querySelector<HTMLElement>('#submitContainer');
    assertTrue(!!container);

    container.dispatchEvent(new FocusEvent('focusin'));
    const event = await focusinPromise;

    assertTrue(!!event);
  });
});

suite('ComposeboxSmartComposeSubmitTest', () => {
  const testProxy = setupComposeboxTest();

  test('populates and passes smartComposeStats on submit', async () => {
    loadTimeData.overrideValues({
      composeboxSmartComposeEnabled: true,
      composeboxShowZps: true,
    });
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const inputElement = testProxy.element.getInputElement().$.input;

    testProxy.element.smartComposeStats = {
      enabled: true,
      shownCount: 0,
      acceptedCount: 0,
      charactersAccepted: 0,
      shownLength: 0,
    };

    // Trigger render containing inline hint.
    const hint = ' hint';
    const matches = [createSearchMatchForTesting({
      fillIntoEdit: 'test',
      allowedToBeDefaultMatch: true,
    })];
    testProxy.element.lastQueriedInput = 'test';
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches,
          smartComposeInlineHint: hint,
        }));
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Verification: shownCount and shownLength populated.
    assertDeepEquals(testProxy.element.smartComposeStats, {
      enabled: true,
      shownCount: 1,
      acceptedCount: 0,
      charactersAccepted: 0,
      shownLength: 5,  // Length of " hint"
    });

    // Trigger acceptance by tabbing.
    inputElement.focus();
    const tabEvent = new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true});
    inputElement.dispatchEvent(tabEvent);
    await microtasksFinished();

    // Verification: acceptedCount and charactersAccepted populated.
    assertDeepEquals(testProxy.element.smartComposeStats, {
      enabled: true,
      shownCount: 1,
      acceptedCount: 1,
      charactersAccepted: 5,
      shownLength: 5,
    });

    // Delivering fresh results including a SECOND hint to verify accumulation.
    const hint2 = ' and more';
    testProxy.element.lastQueriedInput = 'test hint';
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test hint',
          matches,
          smartComposeInlineHint: hint2,
        }));
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Verification: shownCount and shownLength accumulated properly.
    assertDeepEquals(testProxy.element.smartComposeStats, {
      enabled: true,
      shownCount: 2,
      acceptedCount: 1,
      charactersAccepted: 5,
      shownLength: 14,  // 5 + 9 chars
    });

    // Accept the second hint.
    inputElement.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true}));
    await microtasksFinished();

    // Verification: acceptedCount and charactersAccepted accumulated.
    assertDeepEquals(testProxy.element.smartComposeStats, {
      enabled: true,
      shownCount: 2,
      acceptedCount: 2,
      charactersAccepted: 14,
      shownLength: 14,
    });

    // Delivering one last fresh result sets the final state for selection.
    testProxy.element.lastQueriedInput = 'test hint and more';
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test hint and more',
          matches,
        }));
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Select dummy match manually and trigger submit via submit container.
    testProxy.element.getDropdownElement().selectIndex(0);
    await microtasksFinished();

    const submitBtn =
        testProxy.element.shadowRoot.querySelector('cr-composebox-submit');
    assertTrue(!!submitBtn);
    const submitContainer =
        submitBtn.shadowRoot.querySelector<HTMLElement>('#submitContainer');
    assertTrue(!!submitContainer);
    submitContainer.click();
    await microtasksFinished();

    // Verification: correct sequence of calls is triggered.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('setSmartComposeStats'), 1);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 1);

    const passedStats =
        testProxy.searchboxHandler.getArgs('setSmartComposeStats')[0];
    assertDeepEquals(passedStats, {
      enabled: true,
      shownCount: 2,
      acceptedCount: 2,
      charactersAccepted: 14,
      shownLength: 14,
    });
  });
});
