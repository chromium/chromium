// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/user_skills_page.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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

  setup(function() {
    browserProxy = new TestSkillsBrowserProxy();
    SkillsPageBrowserProxy.setInstance(browserProxy);
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    page = document.createElement('user-skills-page');
    document.body.appendChild(page);
    return microtasksFinished();
  });

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
    const testSkill = {
      id: '123',
      name: 'Test Skill',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    browserProxy.callbackRouterRemote.updateSkill(testSkill);
    await microtasksFinished();

    const skillItems = page.shadowRoot.querySelectorAll('li');
    assertEquals(1, skillItems.length);
    assertTrue(!!skillItems[0]);
    assertEquals('Test Skill', skillItems[0].textContent.trim());
  });

  test('RemoveSkillUpdatesPage', async function() {
    const testSkill = {
      id: '123',
      name: 'Test Skill',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    browserProxy.callbackRouterRemote.updateSkill(testSkill);
    await microtasksFinished();

    let skillItems = page.shadowRoot.querySelectorAll('li');
    assertTrue(!!skillItems[0]);
    assertEquals('Test Skill', skillItems[0].textContent.trim());

    browserProxy.callbackRouterRemote.removeSkill(testSkill.id);
    await microtasksFinished();

    skillItems = page.shadowRoot.querySelectorAll('li');
    assertEquals(0, skillItems.length);
  });

  test('UpdatesAndRemovalsShowCorrectly', async function() {
    const skillA = {
      id: '123',
      name: 'A',
      icon: '',
      prompt: '',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };
    const skillB = {
      id: '234',
      name: 'B',
      icon: '',
      prompt: '',
      source: SkillSource.kUserCreated,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
    };

    browserProxy.callbackRouterRemote.updateSkill(skillA);
    browserProxy.callbackRouterRemote.updateSkill(skillB);
    browserProxy.callbackRouterRemote.removeSkill(skillA.id);

    await microtasksFinished();

    const skillItems = page.shadowRoot.querySelectorAll('li');
    assertEquals(1, skillItems.length);
    assertTrue(!!skillItems[0]);
    assertEquals('B', skillItems[0].textContent.trim());
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
});
