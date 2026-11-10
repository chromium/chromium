// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {AiFeedbackDialogElement, AiFeedbackSubmittedDetail} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AiNegativeFeedbackReason} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment} from './common.js';

suite('AiFeedbackDialog', () => {
  let dialog: AiFeedbackDialogElement;
  let shadowRoot: ShadowRoot;

  function getCheckboxes(): NodeListOf<CrCheckboxElement> {
    return shadowRoot.querySelectorAll<CrCheckboxElement>('.reason-checkbox');
  }

  function getCheckbox(reason: AiNegativeFeedbackReason): CrCheckboxElement {
    return shadowRoot.querySelector<CrCheckboxElement>(
        `.reason-checkbox[data-reason="${reason}"]`)!;
  }

  function getSubmitButton(): CrButtonElement {
    return shadowRoot.querySelector<CrButtonElement>('#submitButton')!;
  }

  function getCancelButton(): CrButtonElement {
    return shadowRoot.querySelector<CrButtonElement>('#cancelButton')!;
  }

  async function setCheckbox(
      reason: AiNegativeFeedbackReason, checked: boolean) {
    const checkbox = getCheckbox(reason);
    checkbox.checked = checked;
    checkbox.dispatchEvent(new CustomEvent(
        'checked-changed', {detail: {value: checked}, bubbles: true}));
    await microtasksFinished();
  }

  async function submitAndWait() {
    const whenSubmitted =
        eventToPromise<CustomEvent<AiFeedbackSubmittedDetail>>(
            'ai-feedback-submitted', dialog);
    getSubmitButton().click();
    return (await whenSubmitted).detail;
  }

  setup(async () => {
    setupTestEnvironment();
    dialog = document.createElement('ai-feedback-dialog');
    document.body.appendChild(dialog);
    await microtasksFinished();
    assertTrue(!!dialog.shadowRoot);
    shadowRoot = dialog.shadowRoot;
  });

  test('renders checklist view initially with disabled submit button', () => {
    assertTrue(dialog.$.aiFeedbackDialog.open);
    assertEquals(
        'What went wrong?',
        shadowRoot.querySelector('.dialog-title')?.textContent?.trim());

    const checkboxes = getCheckboxes();
    assertEquals(5, checkboxes.length);
    for (const checkbox of checkboxes) {
      assertFalse(checkbox.checked);
    }

    assertTrue(getSubmitButton().disabled);
  });

  test(
      'selecting and deselecting checkboxes toggles submit button and emits payload',
      async () => {
        await setCheckbox(AiNegativeFeedbackReason.BAD_VOICE, true);
        assertFalse(getSubmitButton().disabled);

        await setCheckbox(AiNegativeFeedbackReason.OFFENSIVE, true);
        await setCheckbox(AiNegativeFeedbackReason.BAD_VOICE, false);
        assertFalse(getSubmitButton().disabled);

        const detail = await submitAndWait();
        assertDeepEquals([AiNegativeFeedbackReason.OFFENSIVE], detail.reasons);
        assertEquals('', detail.otherText);
      });

  test(
      'submit button is disabled again when all checkboxes are unselected',
      async () => {
        await setCheckbox(AiNegativeFeedbackReason.BAD_VOICE, true);
        await setCheckbox(AiNegativeFeedbackReason.TECHNICAL_ISSUE, true);
        assertFalse(getSubmitButton().disabled);

        await setCheckbox(AiNegativeFeedbackReason.BAD_VOICE, false);
        assertFalse(getSubmitButton().disabled);

        await setCheckbox(AiNegativeFeedbackReason.TECHNICAL_ISSUE, false);
        assertTrue(getSubmitButton().disabled);
      });

  test(
      'consumes keydown events inside dialog so shortcuts do not bubble out',
      () => {
        assertTrue(dialog.$.aiFeedbackDialog.consumeKeydownEvent);

        let bubbledToDocument = false;
        const listener = () => {
          bubbledToDocument = true;
        };
        document.addEventListener('keydown', listener);
        try {
          const checkbox = getCheckbox(AiNegativeFeedbackReason.BAD_VOICE);
          checkbox.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'k', bubbles: true, composed: true}));
          checkbox.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowLeft', bubbles: true, composed: true}));
          assertFalse(bubbledToDocument);
        } finally {
          document.removeEventListener('keydown', listener);
        }
      });

  test('cancel button closes dialog and emits close event', async () => {
    const whenClosed = eventToPromise('close', dialog);
    getCancelButton().click();
    await whenClosed;
    assertFalse(dialog.$.aiFeedbackDialog.open);
  });

  test('escape closes dialog', async () => {
    // Synthetic key events do not trigger the native <dialog> behavior, so
    // dispatch the cancel event that pressing Escape produces.
    const whenCancelled = eventToPromise('cancel', dialog);
    dialog.$.aiFeedbackDialog.getNative().dispatchEvent(
        new CustomEvent('cancel', {cancelable: true}));
    await whenCancelled;
    assertFalse(dialog.$.aiFeedbackDialog.open);
  });
});
