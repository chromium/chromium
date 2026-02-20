// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/user_skills_page.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsDialogType} from 'chrome://skills/skills.mojom-webui.js';
import {SkillsPageBrowserProxy} from 'chrome://skills/skills_page_browser_proxy.js';
import type {UserSkillsPageElement} from 'chrome://skills/user_skills_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSkillsBrowserProxy} from './test_skills_browser_proxy.js';

suite('UserSkillsPage', function() {
  let page: UserSkillsPageElement;
  let browserProxy: TestSkillsBrowserProxy;

  setup(async function() {
    browserProxy = new TestSkillsBrowserProxy();
    SkillsPageBrowserProxy.setInstance(browserProxy);
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    page = document.createElement('user-skills-page');
    document.body.appendChild(page);
    await microtasksFinished();
  });

  // Helper to create a valid Skill object with defaults.
  function createSkill(overrides: Partial<Skill> = {}): Skill {
    return {
      id: '1',
      sourceSkillId: null,
      name: 'Default Skill',
      icon: '',
      prompt: '',
      description: '',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
      ...overrides,
    };
  }

  async function setUserSkills(skills: Array<Partial<Skill>>) {
    for (const skill of skills) {
      browserProxy.callbackRouterRemote.updateSkill(createSkill(skill));
    }
    await microtasksFinished();
  }

  test('InitialPageLoadsCorrectly', function() {
    const title = page.$['skillsTitle'];
    assertTrue(!!title);
    assertEquals(
        loadTimeData.getString('userSkillsTitle'), title.textContent.trim());

    const emptyState = page.$['emptyState'];
    assertTrue(!!emptyState);
    const notice = page.shadowRoot.querySelector('.body-text');
    assertTrue(!!notice);
    assertEquals(
        loadTimeData.getString('emptyStateDescription'),
        notice.textContent.trim());
  });

  test('AddSkillButtonTriggersDialog', async function() {
    const addButton = page.$['addSkillButton'];
    assertTrue(!!addButton);
    (addButton as HTMLElement).click();
    const [dialogType, skill] =
        await browserProxy.handler.whenCalled('openSkillsDialog');
    assertEquals(SkillsDialogType.kAdd, dialogType);
    assertEquals(null, skill);
  });

  test('UpdateSkillUpdatesPage', async function() {
    await setUserSkills([{
      id: '123',
      name: 'Test Skill',
      icon: 'icon',
      prompt: 'prompt',
      description: 'description',
      source: SkillSource.kUserCreated,
    }]);

    const skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(1, skillItems.length);
    assertTrue(!!skillItems[0]);
    assertEquals('Test Skill', skillItems[0].skill.name);
  });

  test('RemoveSkillUpdatesPage', async function() {
    await setUserSkills([{
      id: '123',
      name: 'Test Skill',
      icon: 'icon',
      prompt: 'prompt',
      description: 'description',
      source: SkillSource.kUserCreated,
    }]);

    let skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertTrue(!!skillItems[0]);
    assertEquals('Test Skill', skillItems[0].skill.name);

    browserProxy.callbackRouterRemote.removeSkill('123');
    await microtasksFinished();

    skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(0, skillItems.length);
  });

  test('UpdatesAndRemovalsShowCorrectly', async function() {
    await setUserSkills([
      {
        id: '123',
        name: 'A',
        source: SkillSource.kUserCreated,
      },
      {
        id: '234',
        name: 'B',
        source: SkillSource.kUserCreated,
      },
    ]);

    browserProxy.callbackRouterRemote.removeSkill('123');
    await microtasksFinished();

    const skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(1, skillItems.length);
    assertTrue(!!skillItems[0]);
    assertEquals('B', skillItems[0].skill.name);
  });

  test('AddSkillDebouncesClicks', async function() {
    // Scopes to this test, otherwise other tests would have to manually
    // start the timer.
    const mockTimer = new MockTimer();
    mockTimer.install();
    const addButton = page.$['addSkillButton'] as HTMLButtonElement;
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
    addButton.click();
    await page.updateComplete;

    const [dialogType] =
        await browserProxy.handler.whenCalled('openSkillsDialog');
    assertEquals(SkillsDialogType.kAdd, dialogType);
    assertTrue(addButton.disabled);
    mockTimer.tick(1000);
    await page.updateComplete;
    assertFalse(addButton.disabled);
    mockTimer.uninstall();
  });

  test('SkillsFilteredBySearchTerm', async function() {
    await setUserSkills([
      {
        id: '1',
        name: 'Apple',
        prompt: 'A tasty fruit',
        source: SkillSource.kUserCreated,
      },
      {
        id: '2',
        name: 'Banana',
        prompt: 'Yellow fruit',
        source: SkillSource.kUserCreated,
      },
      {
        id: '3',
        name: 'Carrot',
        prompt: 'Orange vegetable',
        source: SkillSource.kUserCreated,
      },
    ]);

    let skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(3, skillItems.length);

    // Search for apple
    page.onSearchChanged('Apple');
    await page.updateComplete;
    skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(1, skillItems.length);
    assertEquals('Apple', skillItems[0]!.skill.name);

    // Search for fruit
    page.onSearchChanged('fruit');
    await page.updateComplete;
    assertEquals(2, page.shadowRoot.querySelectorAll('skill-card').length);

    // Clear search
    page.onSearchChanged('');
    await page.updateComplete;
    assertEquals(3, page.shadowRoot.querySelectorAll('skill-card').length);
  });

  test('ShowsNoSearchResultsPage', async function() {
    await setUserSkills([{
      id: '1',
      name: 'Apple',
      source: SkillSource.kUserCreated,
    }]);

    page.onSearchChanged('Banana');
    await page.updateComplete;
    assertTrue(!!page.shadowRoot.querySelector('error-page'));

    page.onSearchChanged('');
    await page.updateComplete;
    assertFalse(!!page.shadowRoot.querySelector('error-page'));
  });

  test('SkillDeletedFromCardMenu', async function() {
    await setUserSkills([
      {
        id: '1',
        name: 'Apple',
        prompt: 'A tasty fruit',
        source: SkillSource.kUserCreated,
      },
      {
        id: '2',
        name: 'Banana',
        prompt: 'Yellow fruit',
        source: SkillSource.kUserCreated,
      },
      {
        id: '3',
        name: 'Carrot',
        prompt: 'Orange vegetable',
        source: SkillSource.kUserCreated,
      },
    ]);

    let skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(3, skillItems.length);

    // Delete Skill B
    browserProxy.callbackRouterRemote.removeSkill('2');
    await microtasksFinished();
    await page.updateComplete;

    skillItems = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(2, skillItems.length);
    assertEquals('Apple', skillItems[0]!.skill.name);
    assertEquals('Carrot', skillItems[1]!.skill.name);
  });

  test('CopySkillInstructionsToClipboard', async function() {
    await setUserSkills([{
      id: '1',
      name: 'Mister Tony Bark',
      prompt: 'Describe a good dog',
      source: SkillSource.kUserCreated,
    }]);

    const skillCard = page.shadowRoot.querySelector('skill-card');
    assertTrue(!!skillCard);
    const menuButton = skillCard.$.moreButton;
    assertTrue(!!menuButton);
    menuButton.click();
    await page.updateComplete;
    const copyButton = skillCard.$.copyButton;
    assertTrue(!!copyButton);
    copyButton.click();
    await page.updateComplete;

    const clipboardContent = await navigator.clipboard.readText();
    assertEquals('Describe a good dog', clipboardContent);
  });
});
