// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/skills_dialog_app.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import type {SkillsDialogAppElement} from 'chrome://skills/skills_dialog_app.js';
import {SkillsDialogBrowserProxyImpl} from 'chrome://skills/skills_dialog_browser_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import type {TestPageHandler} from './test_skills_dialog_browser_proxy.js';
import {TestSkillsDialogBrowserProxy} from './test_skills_dialog_browser_proxy.js';

suite('SkillsDialogAppPage', function() {
  let skillsDialogApp: SkillsDialogAppElement;
  let browserProxy: TestSkillsDialogBrowserProxy;
  let pageHandler: TestPageHandler;

  setup(function() {
    browserProxy = new TestSkillsDialogBrowserProxy();
    SkillsDialogBrowserProxyImpl.setInstance(browserProxy);
    pageHandler = browserProxy.handler;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
  });

  test('SkillsDialogAppLoads', function() {
    assertEquals(
        'Add Skill',
        skillsDialogApp.shadowRoot.querySelector('h1')!.textContent);
  });

  test('SaveButtonDisabledStates', async function() {
    const saveButton =
        skillsDialogApp.shadowRoot.querySelector<CrButtonElement>(
            '#save-button')!;
    const nameInput =
        skillsDialogApp.shadowRoot.querySelector<CrInputElement>('#name-text')!;
    const instructionsInput =
        skillsDialogApp.shadowRoot.querySelector<CrTextareaElement>(
            '#instructions-text')!;

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
    instructionsInput.dispatchEvent(new CustomEvent(
        'value-changed', {detail: {value: instructionsInput.value}}));
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
    const saveButton =
        skillsDialogApp.shadowRoot.querySelector<CrButtonElement>(
            '#save-button')!;
    const nameInput =
        skillsDialogApp.shadowRoot.querySelector<CrInputElement>('#name-text')!;
    const instructionsInput =
        skillsDialogApp.shadowRoot.querySelector<CrTextareaElement>(
            '#instructions-text')!;

    // Populate the fields to enable the save button.
    const testName = 'test skill';
    const testPrompt = 'test prompt';
    nameInput.value = testName;
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: nameInput.value}}));
    instructionsInput.value = testPrompt;
    instructionsInput.dispatchEvent(new CustomEvent(
        'value-changed', {detail: {value: instructionsInput.value}}));
    await skillsDialogApp.updateComplete;
    assertEquals(false, saveButton.disabled);

    // Click the save button and verify the proxy call.
    saveButton.click();
    const submittedSkill = await pageHandler.whenCalled('submitSkill');
    assertEquals(testName, submittedSkill.name);
    assertEquals(testPrompt, submittedSkill.prompt);
  });
});
