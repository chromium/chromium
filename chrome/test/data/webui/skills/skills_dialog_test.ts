// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/skills_dialog_app.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsDialogType, SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsPromptRefinementOutcome} from 'chrome://skills/skill_metrics.mojom-webui.js';
import {DialogHandlerRemote} from 'chrome://skills/skills.mojom-webui.js';
import {AUTOCOMPLETE_MIN_CHARS, MAX_NAME_CHAR_COUNT, MAX_PROMPT_CHAR_COUNT, REFINE_SKILL_TIMEOUT_MS, WindowProxyImpl} from 'chrome://skills/skills_dialog_app.js';
import type {SkillsDialogAppElement, WindowProxy} from 'chrome://skills/skills_dialog_app.js';
import {SkillsDialogBrowserProxy} from 'chrome://skills/skills_dialog_browser_proxy.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestWindowProxy implements WindowProxy {
  private customTimeouts_ = new Map<number, Function>();
  // Start at a high number to avoid colliding with native setTimeout ids
  private nextTimeoutId_ = 10000;

  setTimeout(handler: TimerHandler, timeout?: number): number {
    if (timeout === REFINE_SKILL_TIMEOUT_MS) {
      const id = this.nextTimeoutId_++;
      this.customTimeouts_.set(id, handler as Function);
      return id;
    }
    return window.setTimeout(handler, timeout);
  }
  runTimeout() {
    const callbacks = Array.from(this.customTimeouts_.values());
    this.customTimeouts_.clear();
    for (const callback of callbacks) {
      callback();
    }
  }

  hasScheduledTimeout(): boolean {
    return this.customTimeouts_.size > 0;
  }
}

suite('SkillsDialogAppPage', function() {
  let skillsDialogApp: SkillsDialogAppElement;
  let dialogHandler: TestMock<DialogHandlerRemote>&DialogHandlerRemote;
  let testWindowProxy: TestWindowProxy;

  setup(async function() {
    loadTimeData.overrideValues({
      MAX_PROMPT_CHAR_COUNT: 20000,
      MAX_NAME_CHAR_COUNT: 100,
      isRefinementEnabled: true,
      isAutocompleteEnabled: true,
    });
    dialogHandler = TestMock.fromClass(DialogHandlerRemote);
    SkillsDialogBrowserProxy.setInstance(
        {handler: dialogHandler} as SkillsDialogBrowserProxy);
    dialogHandler.setResultFor('submitSkill', Promise.resolve({success: true}));
    dialogHandler.setResultFor(
        'refineSkill', Promise.resolve({refinedSkill: createSkill()}));
    dialogHandler.setResultFor(
        'generateNameAndEmoji', Promise.resolve({refinedSkill: createSkill()}));
    dialogHandler.setResultFor(
        'getSignedInEmail', Promise.resolve({email: ''}));
    const emptySkill = createSkill();
    testWindowProxy = new TestWindowProxy();
    WindowProxyImpl.setInstance(testWindowProxy);
    await setupDialogInitialState(emptySkill);
  });

  // Helper to create a valid Skill object with defaults.
  function createSkill(overrides: Partial<Skill> = {}): Skill {
    return {
      id: '',
      sourceSkillId: '',
      name: '',
      icon: '',
      prompt: '',
      description: '',
      curatedBy: '',
      imageUrl: '',
      source: SkillSource.kUnknown,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
      ...overrides,
    };
  }

  async function setupDialogInitialState(
      initialSkill: Skill,
      dialogType: SkillsDialogType = SkillsDialogType.kAdd) {
    dialogHandler.setResultFor('getInitialState', Promise.resolve({
      initialDialogState: {dialogType: dialogType, skill: initialSkill},
    }));
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
    const testSkill = createSkill({
      id: '123',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
    });
    await setupDialogInitialState(testSkill);

    assertEquals(testSkill.name, skillsDialogApp.$.nameText.value);
    assertEquals(testSkill.prompt, skillsDialogApp.$.instructionsText.value);
  });

  test('AddingFirstPartySkill', async function() {
    const testSkill = createSkill({
      id: 'first-party-skill',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
    });
    await setupDialogInitialState(testSkill);

    assertEquals('Add skill', skillsDialogApp.$.header.textContent);
  });

  test('EditingUserCreatedSkill', async function() {
    const testSkill = createSkill({
      id: '123',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
    });
    await setupDialogInitialState(testSkill, SkillsDialogType.kEdit);

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
    const [submittedSkill, refinementOutcome] =
        await dialogHandler.whenCalled('submitSkill');
    assertEquals(SkillsPromptRefinementOutcome.kNotRefined, refinementOutcome);
    assertEquals('', submittedSkill.id);
    assertEquals(testName, submittedSkill.name);
    assertEquals(testPrompt, submittedSkill.prompt);
    assertEquals(SkillSource.kUserCreated, submittedSkill.source);

    // Verify save error message is not shown.
    assertTrue(skillsDialogApp.$.saveErrorContainer.hidden);
  });

  test('SubmitsRemixedSkill', async function() {
    const firstPartySkill = createSkill({
      id: 'first-party-skill',
      sourceSkillId: 'sourceSkillId',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
    });
    await setupDialogInitialState(firstPartySkill);

    // Remix the fields.
    const remixedName = 'remixed skill';
    const remixedPrompt = 'remixed prompt';
    await updateName(remixedName);
    await updateInstructions(remixedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const [submittedSkill, refinementOutcome] =
        await dialogHandler.whenCalled('submitSkill');
    assertEquals(SkillsPromptRefinementOutcome.kNotRefined, refinementOutcome);
    assertEquals('', submittedSkill.id);
    assertEquals(firstPartySkill.id, submittedSkill.sourceSkillId);
    assertEquals(SkillSource.kDerivedFromFirstParty, submittedSkill.source);
    assertEquals(remixedName, submittedSkill.name);
    assertEquals(remixedPrompt, submittedSkill.prompt);
  });

  test('EditUserCreatedSkill', async function() {
    const userCreatedSkill = createSkill({
      id: 'user-created-skill',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
    });
    await setupDialogInitialState(userCreatedSkill, SkillsDialogType.kEdit);

    // Edit the fields.
    const editedName = 'edited skill';
    const editedPrompt = 'edited prompt';
    await updateName(editedName);
    await updateInstructions(editedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const [submittedSkill, refinementOutcome] =
        await dialogHandler.whenCalled('submitSkill');
    assertEquals(SkillsPromptRefinementOutcome.kNotRefined, refinementOutcome);
    assertEquals(userCreatedSkill.id, submittedSkill.id);
    assertEquals(editedName, submittedSkill.name);
    assertEquals(editedPrompt, submittedSkill.prompt);
    assertEquals(SkillSource.kUserCreated, submittedSkill.source);
  });

  test('EditDerivedFromFirstPartySkill', async function() {
    const derivedFromFirstPartySkill = createSkill({
      id: 'derived-from-first-party-skill',
      sourceSkillId: 'first-party-skill',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kDerivedFromFirstParty,
    });
    await setupDialogInitialState(
        derivedFromFirstPartySkill, SkillsDialogType.kEdit);

    // Edit the fields.
    const editedName = 'edited skill';
    const editedPrompt = 'edited prompt';
    await updateName(editedName);
    await updateInstructions(editedPrompt);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const [submittedSkill, refinementOutcome] =
        await dialogHandler.whenCalled('submitSkill');
    assertEquals(SkillsPromptRefinementOutcome.kNotRefined, refinementOutcome);
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

  test('HidesDeleteButtonForAddingSkill', function() {
    assertTrue(skillsDialogApp.$.deleteButton.hidden);
  });

  test('DeleteSkill', async function() {
    const testSkill = createSkill({
      id: '123',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
    });
    await setupDialogInitialState(testSkill, SkillsDialogType.kEdit);

    assertFalse(skillsDialogApp.$.deleteButton.hidden);

    skillsDialogApp.$.deleteButton.click();
    assertEquals(1, dialogHandler.getCallCount('deleteSkill'));
  });

  test('EmojiZeroStateVisibility', async function() {
    // 1. Setup with empty icon
    const emptyIconSkill = createSkill({
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kUserCreated,
    });
    await setupDialogInitialState(emptyIconSkill, SkillsDialogType.kAdd);

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

    await microtasksFinished();
    assertTrue(!!skillsDialogApp.shadowRoot.querySelector('#emojiPicker'));
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
    const [submittedSkill] = await dialogHandler.whenCalled('submitSkill');
    assertEquals('🐶', submittedSkill.icon);
  });

  test('EmojiSelectedUpdatesStateAndClosesPicker', async function() {
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;
    emojiTrigger.click();
    await microtasksFinished();

    const picker =
        skillsDialogApp.shadowRoot.querySelector('skills-emoji-picker');
    assertTrue(!!picker);

    const testEmoji = '😊';
    picker.dispatchEvent(new CustomEvent('emoji-selected', {
      detail: {emoji: testEmoji},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();
    assertEquals(testEmoji, emojiTrigger.value);
    assertFalse(
        !!skillsDialogApp.shadowRoot.querySelector('skills-emoji-picker'));
  });

  test('EmojiInputHandlesEmptyAndAppliesDefaultOnSubmit', async function() {
    const emptyIconSkill = createSkill({
      id: 'empty-icon-skill',
      sourceSkillId: 'sourceSkillId',
      name: 'test skill',
      prompt: 'test prompt',
      description: 'test description',
      source: SkillSource.kFirstParty,
    });
    await setupDialogInitialState(emptyIconSkill, SkillsDialogType.kEdit);

    // Click the save button and verify the proxy call.
    skillsDialogApp.$.saveButton.click();
    const [submittedSkill] = await dialogHandler.whenCalled('submitSkill');
    assertEquals('⚡', submittedSkill.icon);
  });

  test('EmojiPreventsManualTyping', async function() {
    const emojiTrigger = skillsDialogApp.$.emojiTrigger;
    const enterEvent = new KeyboardEvent('keydown', {
      key: 'Enter',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    emojiTrigger.dispatchEvent(enterEvent);
    assertTrue(enterEvent.defaultPrevented);
    await microtasksFinished();
    assertTrue(!!skillsDialogApp.shadowRoot.querySelector('#emojiPicker'));
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

    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: createSkill({prompt: refinedMockText}),
    }));

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

  test('SubmitsRefinedSkillLogsOutcome', async function() {
    await updateName('Test Name');
    await updateInstructions('Original Prompt');

    // 1. Refine
    const refinedText = 'Refined Prompt';
    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: createSkill({prompt: refinedText}),
    }));

    skillsDialogApp.$.iconRefine.click();
    await dialogHandler.whenCalled('refineSkill');
    await microtasksFinished();

    // 2. Submit
    skillsDialogApp.$.saveButton.click();
    await microtasksFinished();

    const [submittedSkill, refinementOutcome] =
        await dialogHandler.whenCalled('submitSkill');
    assertEquals(
        SkillsPromptRefinementOutcome.kUsedRefinedPrompt, refinementOutcome);
    assertEquals(refinedText, submittedSkill.prompt);
  });

  test('RefinementDisabled', async function () {
    loadTimeData.overrideValues({ isRefinementEnabled: false });
    await setupDialogInitialState(createSkill());

    assertNull(skillsDialogApp.shadowRoot.querySelector('#iconRefine'));
    assertNull(skillsDialogApp.shadowRoot.querySelector('#iconUndo'));
    assertNull(skillsDialogApp.shadowRoot.querySelector('#iconRedo'));
  });

  test('RefineUsesOriginalPromptForSubsequentRefines', async function() {
    // 1. Set the initial text
    const originalText = 'Original Prompt';
    await updateInstructions(originalText);

    // 2. Perform the first refinement
    const firstRefinedText = 'AI Refined Prompt 1';
    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: createSkill({prompt: firstRefinedText}),
    }));

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
    dialogHandler.setResultFor('refineSkill', Promise.resolve({
      refinedSkill: createSkill({prompt: secondRefinedText}),
    }));

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

    const accountInfoElement = skillsDialogApp.$.accountInfo;

    assertTrue(!!accountInfoElement);
    assertTrue(accountInfoElement.textContent.includes(testEmail));
  });

  test('RefineShowsErrorOnFailure', async function() {
    const refineBtn = skillsDialogApp.$.iconRefine;
    // Query these elements dynamically in assertion to ensure freshness
    const textareaWrapper = skillsDialogApp.$.textareaWrapper;
    const errorMessage = skillsDialogApp.$.errorMessage;

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
    const errorMessage = skillsDialogApp.$.errorMessage;

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
      refinedSkill: createSkill({prompt: 'Done'}),
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
    const errorMessage = skillsDialogApp.$.errorMessage;

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
      refinedSkill: createSkill({prompt: 'Late Response'}),
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
    dialogHandler.setResultFor('generateNameAndEmoji', Promise.resolve({
      refinedSkill: createSkill({
        name: generatedName,
        icon: generatedIcon,
        prompt: 'refined prompt',
      }),
    }));

    // 2. Initialize a new skill with a prompt but no name/icon.
    const newSkill =
        createSkill({prompt: 'Instruction that triggers auto-gen'});

    // 3. Mount the component
    await setupDialogInitialState(newSkill, SkillsDialogType.kAdd);

    // 4. Assert that values updated automatically
    assertEquals(generatedName, skillsDialogApp.$.nameText.value);
    assertEquals(generatedIcon, skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulatesNameAndIconOnLoadDisabledByFlag', async function() {
    loadTimeData.overrideValues({
      isAutocompleteEnabled: false,
    });

    dialogHandler.setResultFor('generateNameAndEmoji', Promise.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: 'Auto Generated Name',
        icon: '🤖',
        prompt: 'refined prompt',
        description: '',
        source: SkillSource.kUserCreated,
      },
    }));

    const newSkill =
        createSkill({prompt: 'Instruction that triggers auto-gen'});

    await setupDialogInitialState(newSkill, SkillsDialogType.kAdd);

    // Verify it was never called because the flag is off
    assertEquals(0, dialogHandler.getCallCount('generateNameAndEmoji'));
    assertEquals('', skillsDialogApp.$.nameText.value);
    assertEquals('', skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulateDoesNotOverwriteExistingData', async function() {
    // 1. Setup mock response
    dialogHandler.setResultFor('generateNameAndEmoji', Promise.resolve({
      refinedSkill: createSkill({
        name: 'Should Not Be Used',
        icon: '❌',
      }),
    }));

    // 2. Initialize with user-defined name and icon
    const existingName = 'My Custom Name';
    const existingIcon = '✅';
    const customSkill = createSkill({
      name: existingName,
      icon: existingIcon,
      prompt: 'Instructions',
    });

    // 3. Mount
    await setupDialogInitialState(customSkill);

    // 4. Assert values were preserved
    assertEquals(existingName, skillsDialogApp.$.nameText.value);
    assertEquals(existingIcon, skillsDialogApp.$.emojiTrigger.value);
  });

  test('AutoPopulateLoadingState', async function() {
    // 1. Control the promise to check loading state
    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.generateNameAndEmoji = () => resolver.promise;

    const newSkill = createSkill({
      icon: '⚡',
      prompt: 'Instructions',
    });

    // 2. Mount - this triggers the call immediately in connectedCallback
    await setupDialogInitialState(newSkill, SkillsDialogType.kAdd);

    // 3. Assert Loading State: Input should not be visible, Loader should be
    const nameInput = skillsDialogApp.shadowRoot.querySelector('#nameText');
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');

    assertEquals(null, nameInput);
    assertTrue(!!loader);

    // 4. Resolve the request
    resolver.resolve({
      refinedSkill: createSkill({
        name: 'Done',
        icon: '🏁',
      }),
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
    // 1. Setup a hanging promise for generateNameAndEmoji
    const resolver = new PromiseResolver<{refinedSkill: Skill}>();
    dialogHandler.generateNameAndEmoji = () => resolver.promise;

    const newSkill = createSkill({
      icon: '⚡',
      prompt: 'Trigger auto-pop',
    });

    // 2. Mount
    await setupDialogInitialState(newSkill, SkillsDialogType.kAdd);

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
    const preNamedSkill = createSkill({
      name: 'Pre-existing Name',
      icon: '⚡',
      prompt: 'Instructions triggering auto-pop logic',
    });

    // 2. Mount the component.
    await setupDialogInitialState(preNamedSkill, SkillsDialogType.kAdd);

    // 3. Verify that generateNameAndEmoji was NEVER called.
    assertEquals(0, dialogHandler.getCallCount('generateNameAndEmoji'));

    // 4. Verify no loading state is shown and name remains unchanged.
    const loader =
        skillsDialogApp.shadowRoot.querySelector('#nameLoaderContainer');
    assertEquals(null, loader);
    assertEquals('Pre-existing Name', skillsDialogApp.$.nameText.value);
  });

  test('CharLimitErrorDisplaysCorrectly', async function() {
    await updateName('test skill');

    // 1. At or over limit: disabled.
    const longPrompt = 'a'.repeat(MAX_PROMPT_CHAR_COUNT);
    await updateInstructions(longPrompt);

    // Check error message visibility
    const charLimitErrorMessage = skillsDialogApp.$.errorMessage;
    assertTrue(!!charLimitErrorMessage);
    assertFalse(charLimitErrorMessage.hidden);
    assertTrue(skillsDialogApp.$.textareaWrapper.hasAttribute('error'));

    // 2. Under limit again: enabled.
    await updateInstructions('a');
    assertTrue(charLimitErrorMessage.hidden);
    assertFalse(skillsDialogApp.$.textareaWrapper.hasAttribute('error'));
  });

  test('NameCharLimitErrorDisplaysCorrectly', async function() {
    // 1. At or over limit: red border and custom error message appear.
    const longName = 'a'.repeat(MAX_NAME_CHAR_COUNT);
    await updateName(longName);

    const nameErrorMessage = skillsDialogApp.$.nameErrorMessage;
    assertTrue(!!nameErrorMessage);
    assertFalse(nameErrorMessage.hidden);
    assertTrue(skillsDialogApp.$.nameText.hasAttribute('invalid'));

    // 2. Under limit again: returns to normal.
    await updateName('Valid Name');
    assertTrue(nameErrorMessage.hidden);
    assertFalse(skillsDialogApp.$.nameText.hasAttribute('invalid'));
  });

  test('AutocompleteIgnoresShortPromptOnNameFocus', async function() {
    // Type a short prompt, shouldn't trigger
    await updateInstructions('short');
    skillsDialogApp.$.nameText.dispatchEvent(new Event('focus'));
    await microtasksFinished();
    assertEquals(0, dialogHandler.getCallCount('generateNameAndEmoji'));
  });

  test('AutocompleteFetchesOnNameFocus', async function() {
    // Setup mock
    const generatedName = 'Generated Name';
    const generatedIcon = '🧠';
    dialogHandler.setResultFor('generateNameAndEmoji', Promise.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: generatedName,
        icon: generatedIcon,
        prompt: '1'.repeat(AUTOCOMPLETE_MIN_CHARS),
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    }));

    // Type a long prompt
    const longPrompt = '1'.repeat(AUTOCOMPLETE_MIN_CHARS);
    await updateInstructions(longPrompt);

    // Verify it didn't queue a timeout
    assertFalse(testWindowProxy.hasScheduledTimeout());
    assertEquals(0, dialogHandler.getCallCount('generateNameAndEmoji'));

    // Focus the name input
    skillsDialogApp.$.nameText.dispatchEvent(new Event('focus'));
    await microtasksFinished();

    // Verify proxy was called
    await dialogHandler.whenCalled('generateNameAndEmoji');
    await microtasksFinished();

    // Placeholders should be hidden initially (already done above implicitly or
    // explicitly via tests logic but let's keep assertions)
    assertFalse(skillsDialogApp.$.generatedPlaceholder.hidden);
    const generatedNameText = skillsDialogApp.$.generatedNameText.textContent;
    assertEquals(generatedName, generatedNameText);
    assertEquals(generatedIcon, skillsDialogApp.$.emojiTrigger.value);
    assertTrue(skillsDialogApp.$.emojiZeroStateIcon.hidden);
    assertEquals('', skillsDialogApp.$.nameText.placeholder);

    // Ensure icon got the gray styling
    assertTrue(
        skillsDialogApp.$.emojiTrigger.classList.contains('placeholder-icon'));
  });

  test('AutocompleteTabAcceptsSuggestion', async function() {
    // Setup mock
    const generatedName = 'Generated Name';
    const generatedIcon = '🧠';
    dialogHandler.setResultFor('generateNameAndEmoji', Promise.resolve({
      refinedSkill: {
        id: '',
        sourceSkillId: '',
        name: generatedName,
        icon: generatedIcon,
        prompt: '1'.repeat(AUTOCOMPLETE_MIN_CHARS),
        description: '',
        source: SkillSource.kUserCreated,
        creationTime: {internalValue: 0n},
        lastUpdateTime: {internalValue: 0n},
      },
    }));

    const longPrompt = '1'.repeat(AUTOCOMPLETE_MIN_CHARS);
    await updateInstructions(longPrompt);

    // Focus to trigger the fetch
    skillsDialogApp.$.nameText.dispatchEvent(new Event('focus'));
    await dialogHandler.whenCalled('generateNameAndEmoji');
    await microtasksFinished();

    // Tab key event
    const tabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    skillsDialogApp.$.nameText.dispatchEvent(tabEvent);

    await microtasksFinished();

    // Event should be prevented (so focus doesn't actually fly away)
    assertTrue(tabEvent.defaultPrevented);

    // The attributes should be formally set on the skill, removing placeholder
    // styling
    assertEquals(generatedName, skillsDialogApp.$.nameText.value);
    assertEquals(generatedIcon, skillsDialogApp.$.emojiTrigger.value);
    assertFalse(
        skillsDialogApp.$.emojiTrigger.classList.contains('placeholder-icon'));
  });
});
