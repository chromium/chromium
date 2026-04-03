// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from 'chrome://new-tab-page/lazy_load.js';
import type {ComposeboxSubmitElement} from 'chrome://resources/cr_components/composebox/composebox_submit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
