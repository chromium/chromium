// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/skills_dialog_app.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {DialogHandlerRemote} from 'chrome://skills/skills.mojom-webui.js';
import type {SkillsDialogAppElement} from 'chrome://skills/skills_dialog_app.js';
import {SkillsDialogBrowserProxy} from 'chrome://skills/skills_dialog_browser_proxy.js';
import {assertEquals, assertTrue,assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('SkillsDialogAppPage', function() {
  let skillsDialogApp: SkillsDialogAppElement;
  let dialogHandler: TestMock<DialogHandlerRemote>&DialogHandlerRemote;

  setup(async function() {
    dialogHandler = TestMock.fromClass(DialogHandlerRemote);
    SkillsDialogBrowserProxy.setInstance(
        {handler: dialogHandler} as SkillsDialogBrowserProxy);
    dialogHandler.setResultFor(
        'refineSkill', Promise.resolve({refinedSkill: {}}));
    dialogHandler.setResultFor('getInitialSkill', Promise.resolve({skill: {}}));
    dialogHandler.setResultFor(
        'getSignedInEmail', Promise.resolve({email: ''}));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
    await skillsDialogApp.updateComplete;
  });

  test('SkillsDialogAppLoads', function() {
    assertEquals('Add Skill', skillsDialogApp.$['header']!.textContent);
  });

  test('SkillsDialogPrepopulatesInitialSkill', async function() {
    const testSkill: Skill = {
      id: '123',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    dialogHandler.setResultFor(
        'getInitialSkill', Promise.resolve({skill: testSkill}));
    // Re-create the element to pick up the new dialog arguments.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
    await skillsDialogApp.updateComplete;

    assertEquals(
        '⚡',
        skillsDialogApp.shadowRoot
            .querySelector<HTMLInputElement>('.emoji-trigger')!.value);
    assertEquals(
        testSkill.name,
        (skillsDialogApp.$['nameText'] as CrInputElement).value);
    assertEquals(
        testSkill.prompt,
        (skillsDialogApp.$['instructionsText'] as HTMLTextAreaElement).value);
  });

  test('SaveButtonDisabledStates', async function() {
    const saveButton = skillsDialogApp.$['saveButton'] as CrButtonElement;
    const nameInput = skillsDialogApp.$['nameText'] as CrInputElement;
    const instructionsInput =
        skillsDialogApp.$['instructionsText'] as HTMLTextAreaElement;

    // 1. Initial state: disabled.
    assertEquals(true, saveButton.disabled);

    // 2. Name filled, instructions empty: disabled.
    nameInput.value = 'test skill';
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: nameInput.value}}));
    await skillsDialogApp.updateComplete;
    assertEquals(true, saveButton.disabled);

    // 3. Name and instructions filled: enabled.
    instructionsInput.value = 'test prompt';
    instructionsInput.dispatchEvent(new Event('input'));
    await skillsDialogApp.updateComplete;
    assertEquals(false, saveButton.disabled);

    // 4. Name empty, instructions filled: disabled.
    nameInput.value = '';
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: nameInput.value}}));
    await skillsDialogApp.updateComplete;
    assertEquals(true, saveButton.disabled);
  });

  test('SaveButtonSubmitsSkill', async function() {
    const saveButton = skillsDialogApp.$['saveButton'] as CrButtonElement;
    const nameInput = skillsDialogApp.$['nameText'] as CrInputElement;
    const instructionsInput =
        skillsDialogApp.$['instructionsText'] as HTMLTextAreaElement;

    // Populate the fields to enable the save button.
    const testName = 'test skill';
    const testPrompt = 'test prompt';
    nameInput.value = testName;
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: nameInput.value}}));
    instructionsInput.value = testPrompt;
    instructionsInput.dispatchEvent(new Event('input'));
    await skillsDialogApp.updateComplete;
    assertEquals(false, saveButton.disabled);

    // Click the save button and verify the proxy call.
    saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals(testName, submittedSkill.name);
    assertEquals(testPrompt, submittedSkill.prompt);
  });

  test('EmojiTriggerOpensPicker', async function() {
    const emojiTrigger =
        skillsDialogApp.shadowRoot.querySelector<HTMLInputElement>(
            '.emoji-trigger')!;

    emojiTrigger.click();

    await dialogHandler.whenCalled('showEmojiPicker');
  });

  test('EmojiInputUpdatesStateAndSanitizes', async function() {
    const emojiTrigger =
        skillsDialogApp.shadowRoot.querySelector<HTMLInputElement>(
            '.emoji-trigger')!;

    emojiTrigger.value = '⚡🐶';
    emojiTrigger.dispatchEvent(new InputEvent('input'));

    await skillsDialogApp.updateComplete;

    assertEquals('🐶', emojiTrigger.value);

    const saveButton = skillsDialogApp.$['saveButton'] as CrButtonElement;

    const nameInput = skillsDialogApp.$['nameText'] as CrInputElement;
    const instructionsInput =
        skillsDialogApp.$['instructionsText'] as HTMLTextAreaElement;

    nameInput.value = 'name';
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: 'name'}}));
    instructionsInput.value = 'prompt';
    instructionsInput.dispatchEvent(new Event('input'));

    await skillsDialogApp.updateComplete;

    saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals('🐶', submittedSkill.icon);
  });

  test('EmojiInputHandlesEmpty', async function() {
    const emojiTrigger =
        skillsDialogApp.shadowRoot.querySelector<HTMLInputElement>(
            '.emoji-trigger')!;

    emojiTrigger.value = '';
    emojiTrigger.dispatchEvent(new InputEvent('input'));

    await skillsDialogApp.updateComplete;

    assertEquals('⚡', emojiTrigger.value);
  });

  test('EmojiPreventsManualTyping', function() {
    const emojiTrigger =
        skillsDialogApp.shadowRoot.querySelector<HTMLInputElement>(
            '.emoji-trigger')!;

    const letterEvent = new KeyboardEvent('keydown', {
      key: 'a',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    emojiTrigger.dispatchEvent(letterEvent);
    assertTrue(letterEvent.defaultPrevented, 'Should prevent regular keys');
  });

  test('RefineUndoRedoFlow', async function() {
    const instructionsInput =
        skillsDialogApp.$['instructionsText'] as HTMLTextAreaElement;
    const refineBtn =
        skillsDialogApp.shadowRoot.querySelector<CrIconButtonElement>(
            '.icon-refine')!;
    const undoBtn =
        skillsDialogApp.shadowRoot.querySelector<CrIconButtonElement>(
            '.icon-undo')!;
    const redoBtn =
        skillsDialogApp.shadowRoot.querySelector<CrIconButtonElement>(
            '.icon-redo')!;

    // 1. Initial State: Empty
    assertTrue(refineBtn.disabled, 'Refine should be disabled when empty');
    assertTrue(undoBtn.disabled, 'Undo should be disabled initially');
    assertTrue(redoBtn.disabled, 'Redo should be disabled initially');

    // 2. Type something
    const originalText = 'Original Prompt';
    instructionsInput.value = originalText;
    instructionsInput.dispatchEvent(new Event('input'));
    await skillsDialogApp.updateComplete;

    assertFalse(refineBtn.disabled, 'Refine should be enabled after typing');
    assertTrue(undoBtn.disabled);

    // 3. Mock the refine call and Click Refine
    const refinedMockText = 'AI Refined Prompt';

    dialogHandler.refineSkill = (skill: any) => {
      dialogHandler.methodCalled('refineSkill', skill);
      return Promise.resolve({
        refinedSkill: {prompt: refinedMockText} as any,
      });
    };

    refineBtn.click();

    await dialogHandler.whenCalled('refineSkill');

    await new Promise(resolve => setTimeout(resolve, 0));
    await skillsDialogApp.updateComplete;

    assertEquals(
        refinedMockText, instructionsInput.value,
        'Text should be updated to the mocked AI response');

    // Check buttons
    assertFalse(undoBtn.disabled, 'Undo should be enabled after refining');
    assertTrue(redoBtn.disabled, 'Redo should remain disabled');

    // 4. Click Undo
    undoBtn.click();
    await skillsDialogApp.updateComplete;

    assertEquals(
        originalText, instructionsInput.value,
        'Undo should revert to original text');
    assertTrue(undoBtn.disabled, 'Undo should be disabled after undoing');
    assertFalse(redoBtn.disabled, 'Redo should be enabled after undoing');

    // 5. Click Redo
    redoBtn.click();
    await skillsDialogApp.updateComplete;

    assertEquals(
        refinedMockText, instructionsInput.value,
        'Redo should restore refined text');
    assertFalse(undoBtn.disabled, 'Undo should be enabled after redoing');
    assertTrue(redoBtn.disabled, 'Redo should be disabled after redoing');

    // 6. Manual edit clears history
    instructionsInput.value = 'New manual edit';
    instructionsInput.dispatchEvent(new Event('input'));
    await skillsDialogApp.updateComplete;

    assertTrue(undoBtn.disabled, 'Manual edit should clear undo state');
    assertTrue(redoBtn.disabled, 'Manual edit should clear redo state');
  });

  test('DisplaysSignedInEmail', async function() {
    const testEmail = 'user@example.com';
    dialogHandler.setResultFor(
        'getSignedInEmail', Promise.resolve({email: testEmail}));

    // Re-create the element to trigger connectedCallback
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);

    await dialogHandler.whenCalled('getSignedInEmail');

    await skillsDialogApp.updateComplete;

    const emailElement = skillsDialogApp.$['accountEmail'];

    assertTrue(!!emailElement, 'Email element should exist');
    assertEquals(testEmail, emailElement.textContent);
  });
});
