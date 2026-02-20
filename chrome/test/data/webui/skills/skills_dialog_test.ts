// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/skills_dialog_app.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {DialogHandlerRemote} from 'chrome://skills/skills.mojom-webui.js';
import {WindowProxyImpl} from 'chrome://skills/skills_dialog_app.js';
import type {SkillsDialogAppElement, WindowProxy} from 'chrome://skills/skills_dialog_app.js';
import {SkillsDialogBrowserProxy} from 'chrome://skills/skills_dialog_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestWindowProxy implements WindowProxy {
  private callback_: Function|null = null;

  setTimeout(handler: TimerHandler, timeout?: number): number {
    if (timeout === 5000) {
      this.callback_ = handler as Function;
      return 12345;
    }
    return window.setTimeout(handler, timeout);
  }

  runTimeout() {
    if (this.callback_) {
      this.callback_();
    }
  }

  hasScheduledTimeout(): boolean {
    return !!this.callback_;
  }
}

suite('SkillsDialogAppPage', function() {
  let skillsDialogApp: SkillsDialogAppElement;
  let dialogHandler: TestMock<DialogHandlerRemote>&DialogHandlerRemote;
  let testWindowProxy: TestWindowProxy;

  setup(async function() {
    dialogHandler = TestMock.fromClass(DialogHandlerRemote);
    SkillsDialogBrowserProxy.setInstance(
        {handler: dialogHandler} as SkillsDialogBrowserProxy);
    dialogHandler.setResultFor('submitSkill', Promise.resolve({success: true}));
    dialogHandler.setResultFor(
        'refineSkill', Promise.resolve({refinedSkill: {}}));
    dialogHandler.setResultFor(
        'getSignedInEmail', Promise.resolve({email: ''}));
    const emptySkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: '',
      icon: '',
      prompt: '',
      description: '',
      source: SkillSource.kUnknown,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    testWindowProxy = new TestWindowProxy();
    WindowProxyImpl.setInstance(testWindowProxy);
    await setupDialogWithSkill(emptySkill);
  });

  async function setupDialogWithSkill(initialSkill: Skill) {
    dialogHandler.setResultFor(
        'getInitialSkill', Promise.resolve({skill: initialSkill}));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
    return microtasksFinished();
  }

  async function updateName(name: string) {
    const nameInput = skillsDialogApp.$.nameText;

    nameInput.value = name;
    nameInput.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: nameInput.value}}));
    return microtasksFinished();
  }

  async function updateInstructions(prompt: string) {
    const instructionsInput = skillsDialogApp.$.instructionsText;

    instructionsInput.value = prompt;
    instructionsInput.dispatchEvent(new Event('input'));
    return microtasksFinished();
  }

  test('SkillsDialogAppLoads', function() {
    assertEquals('Add skill', skillsDialogApp.$.header.textContent);
  });

  test('SkillsDialogPrepopulatesInitialSkill', async function() {
    const testSkill: Skill = {
      id: '123',
      sourceSkillId: '',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(testSkill);

    assertEquals(testSkill.name, skillsDialogApp.$.nameText.value);
    assertEquals(testSkill.prompt, skillsDialogApp.$.instructionsText.value);
  });

  test('AddingFirstPartySkill', async function() {
    const testSkill: Skill = {
      id: 'first-party-skill',
      sourceSkillId: '',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(testSkill);

    assertEquals('Add skill', skillsDialogApp.$.header.textContent);
  });

  test('EditingUserCreatedSkill', async function() {
    const testSkill: Skill = {
      id: '123',
      sourceSkillId: '',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(testSkill);

    assertEquals('Edit skill', skillsDialogApp.$.header.textContent);
  });

  test('SaveButtonDisabledStates', async function() {
    const saveButton = skillsDialogApp.$.saveButton;

    // 1. Initial state: disabled.
    assertTrue(saveButton.disabled);

    // 2. Name filled, instructions empty: disabled.
    await updateName('test skill');
    assertTrue(saveButton.disabled);

    // 3. Name and instructions filled: enabled.
    await updateInstructions('test prompt');
    assertFalse(saveButton.disabled);

    // 4. Name empty, instructions filled: disabled.
    await updateName('');
    assertTrue(saveButton.disabled);
  });

  test('SaveButtonSubmitsSkill', async function() {
    // Populate the fields to enable the save button.
    const testName = 'test skill';
    const testPrompt = 'test prompt';
    await updateName(testName);
    await updateInstructions(testPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    await microtasksFinished();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals('', submittedSkill.id);
    assertEquals(testName, submittedSkill.name);
    assertEquals(testPrompt, submittedSkill.prompt);
    assertEquals(SkillSource.kUserCreated, submittedSkill.source);

    // Verify save error message is not shown.
    assertTrue(skillsDialogApp.$.saveErrorContainer.hidden);
  });

  test('SubmitsRemixedSkill', async function() {
    const firstPartySkill: Skill = {
      id: 'first-party-skill',
      sourceSkillId: 'sourceSkillId',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(firstPartySkill);

    // Remix the fields.
    const remixedName = 'remixed skill';
    const remixedPrompt = 'remixed prompt';
    await updateName(remixedName);
    await updateInstructions(remixedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals('', submittedSkill.id);
    assertEquals(firstPartySkill.id, submittedSkill.sourceSkillId);
    assertEquals(SkillSource.kDerivedFromFirstParty, submittedSkill.source);
    assertEquals(remixedName, submittedSkill.name);
    assertEquals(remixedPrompt, submittedSkill.prompt);
  });

  test('EditUserCreatedSkill', async function() {
    const userCreatedSkill: Skill = {
      id: 'user-created-skill',
      sourceSkillId: '',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(userCreatedSkill);

    // Edit the fields.
    const editedName = 'edited skill';
    const editedPrompt = 'edited prompt';
    await updateName(editedName);
    await updateInstructions(editedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals(userCreatedSkill.id, submittedSkill.id);
    assertEquals(editedName, submittedSkill.name);
    assertEquals(editedPrompt, submittedSkill.prompt);
    assertEquals(SkillSource.kUserCreated, submittedSkill.source);
  });

  test('EditDerivedFromFirstPartySkill', async function() {
    const derivedFromFirstPartySkill: Skill = {
      id: 'derived-from-first-party-skill',
      sourceSkillId: 'first-party-skill',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kDerivedFromFirstParty,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(derivedFromFirstPartySkill);

    // Edit the fields.
    const editedName = 'edited skill';
    const editedPrompt = 'edited prompt';
    await updateName(editedName);
    await updateInstructions(editedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals(derivedFromFirstPartySkill.id, submittedSkill.id);
    assertEquals(editedName, submittedSkill.name);
    assertEquals(editedPrompt, submittedSkill.prompt);
    assertEquals(SkillSource.kDerivedFromFirstParty, submittedSkill.source);
  });

  test('SaveButtonFails', async function() {
    dialogHandler.setResultFor(
        'submitSkill', Promise.resolve({success: false}));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
    await microtasksFinished();

    // Verify save error message is not shown initially.
    assertTrue(skillsDialogApp.$.saveErrorContainer.hidden);

    // Populate the fields to enable the save button.
    const testName = 'test skill';
    const testPrompt = 'test prompt';
    await updateName(testName);
    await updateInstructions(testPrompt);

    // Verify save error message is shown.
    skillsDialogApp.$.saveButton.click();
    await microtasksFinished();
    await dialogHandler.whenCalled('submitSkill');
    assertFalse(skillsDialogApp.$.saveErrorContainer.hidden);
  });


  test('EmojiZeroStateVisibility', async function() {
    // 1. Setup with empty icon
    const emptyIconSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(emptyIconSkill);

    const zeroStateIcon = skillsDialogApp.$.emojiZeroStateIcon;
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;

    // Verify initial state: Icon should be visible (hidden=false) because icon
    // is empty.
    assertTrue(!!zeroStateIcon);
    assertFalse(zeroStateIcon.hidden);
    assertEquals('', emojiTrigger.value);

    // 2. Set an icon
    emojiTrigger.value = '🐶';
    emojiTrigger.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    // Verify state: Icon should be hidden
    assertTrue(zeroStateIcon.hidden);
    assertEquals('🐶', emojiTrigger.value);
  });

  test('EmojiTriggerOpensPicker', async function() {
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;

    emojiTrigger.click();

    await dialogHandler.whenCalled('showEmojiPicker');
  });

  test('EmojiInputUpdatesStateAndSanitizes', async function() {
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;

    emojiTrigger.value = '⚡🐶';
    emojiTrigger.dispatchEvent(new InputEvent('input'));

    await microtasksFinished();

    assertEquals('🐶', emojiTrigger.value);

    await updateName('name');
    await updateInstructions('prompt');

    skillsDialogApp.$.saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals('🐶', submittedSkill.icon);
  });

  test('EmojiInputHandlesEmptyAndAppliesDefaultOnSubmit', async function() {
    const emptyIconSkill: Skill = {
      id: 'empty-icon-skill',
      sourceSkillId: 'sourceSkillId',
      name: 'test skill',
      icon: '',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    await setupDialogWithSkill(emptyIconSkill);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const submittedSkill = await dialogHandler.whenCalled('submitSkill');
    assertEquals('⚡', submittedSkill.icon);
  });

  test('EmojiPreventsManualTyping', async function() {
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;
    const letterEvent = new KeyboardEvent('keydown', {
      key: 'a',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    emojiTrigger.dispatchEvent(letterEvent);
    assertTrue(letterEvent.defaultPrevented);

    const tabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    emojiTrigger.dispatchEvent(tabEvent);
    assertFalse(tabEvent.defaultPrevented);

    const enterEvent = new KeyboardEvent('keydown', {
      key: 'Enter',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    emojiTrigger.dispatchEvent(enterEvent);
    assertTrue(enterEvent.defaultPrevented);
    await dialogHandler.whenCalled('showEmojiPicker');
  });

  test('RefineUndoRedoFlow', async function() {
    // 1. Initial State: Empty
    assertTrue(skillsDialogApp.$.iconRefine.disabled);
    assertTrue(skillsDialogApp.$.iconUndo.disabled);
    assertTrue(skillsDialogApp.$.iconRedo.disabled);

    // 2. Type something
    const originalText = 'Original Prompt';
    await updateInstructions(originalText);

    assertFalse(skillsDialogApp.$.iconRefine.disabled);
    assertTrue(skillsDialogApp.$.iconUndo.disabled);

    // 3. Mock the refine call and Click Refine
    const refinedMockText = 'AI Refined Prompt';

    dialogHandler.setResultFor(
        'refineSkill',
        Promise.resolve({refinedSkill: {prompt: refinedMockText}}));

    skillsDialogApp.$.iconRefine.click();
    await dialogHandler.whenCalled('refineSkill');
    await microtasksFinished();

    // Accessing the element via the $ map fails here because the element is
    // conditionally rendered and may be removed from the DOM during the
    // loading state. Querying shadowRoot directly allows us to safely check
    // its current state.
    let instructionsInput =
        skillsDialogApp.shadowRoot.querySelector('textarea');
    assertTrue(!!instructionsInput);
    assertEquals(refinedMockText, instructionsInput.value);

    // Check buttons
    assertFalse(skillsDialogApp.$.iconUndo.disabled);
    assertTrue(skillsDialogApp.$.iconRedo.disabled);

    // 4. Click Undo
    skillsDialogApp.$.iconUndo.click();
    await microtasksFinished();

    instructionsInput = skillsDialogApp.shadowRoot.querySelector('textarea');
    assertTrue(!!instructionsInput);
    assertEquals(originalText, instructionsInput.value);
    assertTrue(skillsDialogApp.$.iconUndo.disabled);
    assertFalse(skillsDialogApp.$.iconRedo.disabled);

    // 5. Click Redo
    skillsDialogApp.$.iconRedo.click();
    await microtasksFinished();

    instructionsInput = skillsDialogApp.shadowRoot.querySelector('textarea');
    assertTrue(!!instructionsInput);
    assertEquals(refinedMockText, instructionsInput.value);
    assertFalse(skillsDialogApp.$.iconUndo.disabled);
    assertTrue(skillsDialogApp.$.iconRedo.disabled);

    // 6. Manual edit clears history
    await updateInstructions('New manual edit');

    assertTrue(skillsDialogApp.$.iconUndo.disabled);
    assertTrue(skillsDialogApp.$.iconRedo.disabled);
  });

  test('RefineUsesOriginalPromptForSubsequentRefines', async function() {
    // 1. Set the initial text
    const originalText = 'Original Prompt';
    await updateInstructions(originalText);

    // 2. Perform the first refinement
    const firstRefinedText = 'AI Refined Prompt 1';
    dialogHandler.setResultFor(
        'refineSkill',
        Promise.resolve({refinedSkill: {prompt: firstRefinedText}}));

    skillsDialogApp.$.iconRefine.click();

    let calledSkill = await dialogHandler.whenCalled('refineSkill');
    assertEquals(originalText, calledSkill.prompt);

    await microtasksFinished();
    assertEquals(
        firstRefinedText,
        skillsDialogApp.shadowRoot
            .querySelector<HTMLTextAreaElement>('#instructionsText')!.value);

    dialogHandler.resetResolver('refineSkill');

    // 3. Perform a second refinement immediately after (without manual edits)
    const secondRefinedText = 'AI Refined Prompt 2';
    dialogHandler.setResultFor(
        'refineSkill',
        Promise.resolve({refinedSkill: {prompt: secondRefinedText}}));

    skillsDialogApp.$.iconRefine.click();

    calledSkill = await dialogHandler.whenCalled('refineSkill');
    assertEquals(originalText, calledSkill.prompt);

    await microtasksFinished();
    assertEquals(
        secondRefinedText,
        skillsDialogApp.shadowRoot
            .querySelector<HTMLTextAreaElement>('#instructionsText')!.value);
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

    await microtasksFinished();

    const emailElement = skillsDialogApp.$.accountEmail;

    assertTrue(!!emailElement);
    assertEquals(testEmail, emailElement.textContent);
  });

  test('RefineShowsErrorOnFailure', async function() {
    const refineBtn = skillsDialogApp.$.iconRefine;
    // Query these elements dynamically in assertion to ensure freshness
    const textareaWrapper = skillsDialogApp.$.textareaWrapper;
    const errorMessage = skillsDialogApp.$.refineErrorMessage;

    // 1. Setup Input
    await updateInstructions('Start text');

    // 2. Mock Failure
    dialogHandler.setPromiseRejectFor('refineSkill');

    // 3. Click Refine
    refineBtn.click();
    await microtasksFinished();

    // 4. Assert Error UI
    assertFalse(errorMessage.hidden);
    assertTrue(textareaWrapper.hasAttribute('error'));
  });

  test('TypingClearsRefineError', async function() {
    const refineBtn = skillsDialogApp.$.iconRefine;

    // Helper functions to get fresh DOM elements
    const textareaWrapper = skillsDialogApp.$.textareaWrapper;
    const errorMessage = skillsDialogApp.$.refineErrorMessage;

    // 1. Setup Input and Trigger Error
    await updateInstructions('Start');
    dialogHandler.setPromiseRejectFor('refineSkill');
    refineBtn.click();
    await microtasksFinished();

    // Verify error is there
    assertFalse(errorMessage.hidden);
    assertTrue(textareaWrapper.hasAttribute('error'));

    // 2. Type in box
    await updateInstructions('Start Editing');

    // 3. Assert Error Cleared using fresh getters
    assertTrue(errorMessage.hidden);
    assertFalse(textareaWrapper.hasAttribute('error'));
  });

  test('RefineLoadingState', async function() {
    const refineBtn = skillsDialogApp.$.iconRefine;

    // 1. Setup Input
    await updateInstructions('Start');

    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.refineSkill = () => resolver.promise;

    // 2. Click Refine
    refineBtn.click();
    await microtasksFinished();

    // 3. ASSERT LOADING STATE
    assertTrue(refineBtn.disabled);
    assertTrue(skillsDialogApp.$.iconUndo.disabled);
    assertTrue(skillsDialogApp.$.iconRedo.disabled);
    assertTrue(skillsDialogApp.$.textareaWrapper.hasAttribute('loading'));
    const textarea =
        skillsDialogApp.shadowRoot.querySelector('#instructionsText');
    assertEquals(null, textarea);
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#instructionsLoader');
    assertTrue(!!loader);

    // 4. Resolve Request
    resolver.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: '',
        icon: '',
        prompt: 'Done',
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    });

    await microtasksFinished();

    // 5. ASSERT NORMAL STATE
    assertFalse(refineBtn.disabled);
    assertFalse(skillsDialogApp.$.iconUndo.disabled);
    assertFalse(skillsDialogApp.$.textareaWrapper.hasAttribute('loading'));
    const loaderAfter =
        skillsDialogApp.shadowRoot.querySelector('#instructionsLoader');
    assertEquals(null, loaderAfter);
    const textareaAfter =
        skillsDialogApp.shadowRoot.querySelector('#instructionsText');
    assertTrue(!!textareaAfter);
    assertEquals('Done', (textareaAfter as HTMLTextAreaElement).value);
  });

  test('LateResponseDoesNotOverwriteError', async function() {
    const refineBtn = skillsDialogApp.$.iconRefine;
    const textareaWrapper = skillsDialogApp.$.textareaWrapper;
    const errorMessage = skillsDialogApp.$.refineErrorMessage;

    // 1. Setup Initial State
    await updateInstructions('Original Text');

    // 2. Mock hanging response
    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.refineSkill = () => resolver.promise;

    // 3. Click Refine
    refineBtn.click();
    await microtasksFinished();

    // Verify proxy captured the call
    assertTrue(testWindowProxy.hasScheduledTimeout());

    // 4. Trigger the Timeout Manually via Proxy
    testWindowProxy.runTimeout();

    await microtasksFinished();

    // 5. Verify Error UI
    assertFalse(errorMessage.hidden);
    assertTrue(textareaWrapper.hasAttribute('error'));
    assertFalse(textareaWrapper.hasAttribute('loading'));
    const textarea =
        skillsDialogApp.shadowRoot.querySelector('#instructionsText');
    assertTrue(!!textarea);

    // 6. Resolve the "Late" Response
    resolver.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: '',
        icon: '',
        prompt: 'Late Response',
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    });

    await microtasksFinished();

    // 7. Verify the "Late" response was IGNORED
    assertEquals('Original Text', (textarea as HTMLTextAreaElement).value);
    assertFalse(errorMessage.hidden);
  });

  test('AutoPopulatesNameAndIconOnLoad', async function() {
    // 1. Setup the mock response for the auto-population call.
    const generatedName = 'Auto Generated Name';
    const generatedIcon = '🤖';
    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: generatedName,
        icon: generatedIcon,
        prompt: 'refined prompt',
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    }));

    // 2. Initialize a new skill with a prompt but no name/icon.
    const newSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: '',
      icon: '⚡',  // Default
      prompt: 'Instruction that triggers auto-gen',
      description: '',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    // 3. Mount the component
    await setupDialogWithSkill(newSkill);

    // 4. Assert that values updated automatically
    assertEquals(generatedName, skillsDialogApp.$.nameText.value);
    assertEquals(generatedIcon, skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulateDoesNotOverwriteExistingData', async function() {
    // 1. Setup mock response
    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: {
        name: 'Should Not Be Used',
        icon: '❌',
        prompt: '',
      },
    }));

    // 2. Initialize with user-defined name and icon
    const existingName = 'My Custom Name';
    const existingIcon = '✅';
    const customSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: existingName,
      icon: existingIcon,
      prompt: 'Instructions',
      source: SkillSource.kUserCreated,
      description: '',
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    // 3. Mount
    await setupDialogWithSkill(customSkill);

    // 4. Assert values were preserved
    assertEquals(existingName, skillsDialogApp.$.nameText.value);
    assertEquals(existingIcon, skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulateLoadingState', async function() {
    // 1. Control the promise to check loading state
    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.refineSkill = () => resolver.promise;

    const newSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: '',
      icon: '⚡',
      prompt: 'Instructions',
      source: SkillSource.kUserCreated,
      description: '',
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    // 2. Mount - this triggers the call immediately in connectedCallback
    await setupDialogWithSkill(newSkill);

    // 3. Assert Loading State: Input should not be visible, Loader should be
    const nameInput = skillsDialogApp.shadowRoot.querySelector('#nameText');
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');

    assertEquals(null, nameInput);
    assertTrue(!!loader);

    // 4. Resolve the request
    resolver.resolve({
      refinedSkill: {
        name: 'Done',
        icon: '🏁',
        prompt: '',
        id: '',
        sourceSkillId: '',
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    });

    await microtasksFinished();

    // 5. Assert Normal State: Input visible, Loader gone
    const nameInputAfter =
        skillsDialogApp.shadowRoot.querySelector('#nameText');
    const loaderAfter =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');

    assertTrue(!!nameInputAfter);
    assertEquals(null, loaderAfter);
    assertEquals('Done', (nameInputAfter as CrInputElement).value);
  });

  test('AutoPopulateTimesOut', async function() {
    // 1. Setup a hanging promise for refineSkill
    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.refineSkill = () => resolver.promise;

    const newSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: '',
      icon: '⚡',
      prompt: 'Trigger auto-pop',
      description: '',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    // 2. Mount
    await setupDialogWithSkill(newSkill);

    // 3. Assert Loading State
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');
    assertTrue(!!loader);
    assertTrue(testWindowProxy.hasScheduledTimeout());

    // 4. Trigger Timeout
    testWindowProxy.runTimeout();
    await microtasksFinished();

    // 5. Assert "Silently Fail" / Normal UI State (No Error)
    const loaderAfter =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');
    assertEquals(null, loaderAfter);

    const nameInput = skillsDialogApp.shadowRoot.querySelector('#nameText');
    assertTrue(!!nameInput);

    // Values should remain defaults
    assertEquals('', (nameInput as CrInputElement).value);
    assertEquals('⚡', skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulateSkippedWhenNameIsNotEmpty', async function() {
    // 1. Define a skill that looks like a "new" skill (no ID) so it triggers
    // the "Add" flow, but give it a name to test the guard clause.
    const preNamedSkill: Skill = {
      id: '',
      sourceSkillId: '',
      name: 'Pre-existing Name',
      icon: '⚡',
      prompt: 'Instructions triggering auto-pop logic',
      source: SkillSource.kUserCreated,
      description: '',
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    // 2. Mount the component.
    await setupDialogWithSkill(preNamedSkill);

    // 3. Verify that refineSkill was NEVER called.
    assertEquals(0, dialogHandler.getCallCount('refineSkill'));

    // 4. Verify no loading state is shown and name remains unchanged.
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');
    assertEquals(null, loader);
    assertEquals('Pre-existing Name', skillsDialogApp.$.nameText.value);
  });
});
