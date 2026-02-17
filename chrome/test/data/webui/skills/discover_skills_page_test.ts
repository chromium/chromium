// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/discover_skills_page.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {DiscoverSkillsPageElement} from 'chrome://skills/discover_skills_page.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsPageBrowserProxy} from 'chrome://skills/skills_page_browser_proxy.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSkillsBrowserProxy} from './test_skills_browser_proxy.js';

suite('DiscoverSkillsPage', function() {
  let page: DiscoverSkillsPageElement;
  let browserProxy: TestSkillsBrowserProxy;

  setup(async function() {
    browserProxy = new TestSkillsBrowserProxy();
    SkillsPageBrowserProxy.setInstance(browserProxy);
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    page = document.createElement('discover-skills-page');
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
      source: SkillSource.kFirstParty,
      creationTime: {internalValue: 0n},
      lastUpdateTime: {internalValue: 0n},
      ...overrides,
    };
  }

  // Helper to update the 1P map and wait for the UI to settle.
  // Accepts a simple object: { "Category": [{ PartialSkill }] }
  async function setFirstPartySkills(
      data: Record<string, Array<Partial<Skill>>>) {
    const fullSkillsMap: Record<string, Skill[]> = {};

    for (const [category, skills] of Object.entries(data)) {
      fullSkillsMap[category] = skills.map(createSkill);
    }

    browserProxy.callbackRouterRemote.update1PMap(fullSkillsMap);
    await microtasksFinished();
  }

  test('DiscoverSkillsPageLoadsCorrectly', async function() {
    await setFirstPartySkills({
      'Top Pick': [{id: '1', name: 'Top Skill'}],
      'Writing': [{id: '2', name: 'Write'}],
    });

    const titles = page.shadowRoot.querySelectorAll('.page-title');
    assertEquals(2, titles.length);
    assertTrue(!!titles[0]);
    assertTrue(!!titles[1]);
    assertEquals(
        loadTimeData.getString('topPicksTitle'), titles[0].textContent.trim());
    assertEquals(
        loadTimeData.getString('browseSkillsTitle'),
        titles[1].textContent.trim());
  });

  test('DiscoverSkillsPageIsBlankWhenMapIsEmpty', async function() {
    await setFirstPartySkills({});

    // Discover skills title should not show without pertinent skills.
    const titles = page.shadowRoot.querySelectorAll('.page-title');
    assertEquals(0, titles.length);
    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertEquals(0, chips.length);
    const cards = page.shadowRoot.querySelectorAll('li');
    assertEquals(0, cards.length);
  });

  test('DefaultChipShowsAllSkills', async function() {
    await setFirstPartySkills({
      'Shopping': [{id: '1', name: 'Shopping'}],
      'Writing': [{id: '2', name: 'Writing'}],
    });

    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertTrue(!!chips);
    // All, Shopping, Writing
    assertEquals(3, chips.length);
    assertTrue(!!chips[0]);
    assertTrue(!!chips[1]);
    const firstIcon = chips[0].querySelector('cr-icon');
    assertTrue(!!firstIcon);

    assertTrue(chips[0].selected);
    assertEquals('cr:check', firstIcon.icon);
    // All skill cards should be shown.
    assertEquals(2, page.shadowRoot.querySelectorAll('skill-card').length);

    chips[1].click();
    await microtasksFinished();
    // Only Shopping skill card should be shown.
    assertEquals(1, page.shadowRoot.querySelectorAll('skill-card').length);
  });

  test('ChipClickTogglesIcon', async function() {
    await setFirstPartySkills({
      'Shopping': [{id: '1', name: 'Shopping'}],
    });

    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertTrue(!!chips);
    // All, Shopping
    assertEquals(2, chips.length);
    assertTrue(!!chips[0]);
    assertTrue(!!chips[1]);
    const firstIcon = chips[0].querySelector('cr-icon');
    assertTrue(!!firstIcon);
    const secondIcon = chips[1].querySelector('cr-icon');
    assertTrue(!!secondIcon);

    assertTrue(chips[0].selected);
    assertEquals('cr:check', firstIcon.icon);
    assertFalse(chips[1].selected);
    assertNotEquals('cr:check', secondIcon.icon);

    chips[1].click();
    await microtasksFinished();

    assertFalse(chips[0].selected);
    assertNotEquals('cr:check', firstIcon.icon);
    assertTrue(chips[1].selected);
    assertEquals('cr:check', secondIcon.icon);
  });

  test('SaveButtonClickDisablesCardsSaveFunctionality', async function() {
    const saveResolver = new PromiseResolver();
    browserProxy.handler.setResultFor('maybeSave1PSkill', saveResolver.promise);

    await setFirstPartySkills({
      'Shopping': [{id: '1', name: 'Shopping'}, {id: '2', name: 'Shopping2'}],
    });

    // Both buttons start enabled
    const cards = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(2, cards.length);
    assertTrue(!!cards[0]);
    const card = cards[0];
    await card.updateComplete;
    const saveButton1 = card.$.saveButton;
    assertFalse(saveButton1.disabled);

    assertTrue(!!cards[1]);
    const card1 = cards[1];
    await card1.updateComplete;
    const saveButton2 = card1.$.saveButton;
    assertFalse(saveButton2.disabled);

    saveButton1.click();
    await microtasksFinished();

    // Disabled before promise resolves
    assertTrue(saveButton1.disabled, 'Button 1 should be disabled during save');
    assertTrue(saveButton2.disabled, 'Button 2 should be disabled during save');

    // Enabled after promise
    saveResolver.resolve({success: true});
    await microtasksFinished();
    assertFalse(
        saveButton1.disabled, 'Button 1 should be re-enabled after save');
    assertFalse(
        saveButton2.disabled, 'Button 2 should be re-enabled after save');
  });

  test('SaveButtonSuccessShowsDialog', async function() {
    await setFirstPartySkills({
      'Shopping': [{id: '1', name: 'Shopping'}],
    });

    const cards = page.shadowRoot.querySelectorAll('skill-card');
    assertTrue(!!cards[0]);
    const card = cards[0];
    await card.updateComplete;

    const saveButton1 = card.$.saveButton;
    assertFalse(saveButton1.disabled);

    saveButton1.click();
    await microtasksFinished();

    const callCount = browserProxy.handler.getCallCount('openSkillsDialog');
    assertEquals(1, callCount);
  });

  test('SaveButtonFailureShowsToast', async function() {
    browserProxy.handler.setResultFor(
        'maybeSave1PSkill', Promise.resolve({success: false}));

    await setFirstPartySkills({
      'Shopping': [{id: '1', name: 'Shopping'}],
    });

    const cards = page.shadowRoot.querySelectorAll('skill-card');
    assertTrue(!!cards[0]);
    const card = cards[0];
    await card.updateComplete;

    const saveButton1 = card.$.saveButton;
    assertFalse(saveButton1.disabled);

    saveButton1.click();
    await microtasksFinished();

    const toast = page.shadowRoot.querySelector('#invalidSkillToast');
    assertTrue((toast as CrToastElement).open, 'Toast should be visible');
    assertTrue(saveButton1.disabled);
  });

  test('SkillsFilteredBySearchTerm', async function() {
    await setFirstPartySkills({
      'Produce': [
        {id: '1', name: 'Apple', description: 'A tasty fruit'},
        {id: '2', name: 'Banana', description: 'Yellow fruit'},
        {id: '3', name: 'Carrot', description: 'Orange vegetable'},
      ],
    });
    let cards = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(3, cards.length);

    // Search apple
    page.onSearchChanged('Apple');
    await microtasksFinished();
    cards = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(1, cards.length);
    assertTrue(cards[0]!.$.name.textContent.includes('Apple'));

    // Search fruit
    page.onSearchChanged('fruit');
    await microtasksFinished();
    cards = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(2, cards.length);

    // Clear search
    page.onSearchChanged('');
    await microtasksFinished();
    cards = page.shadowRoot.querySelectorAll('skill-card');
    assertEquals(3, cards.length);
  });

  test('ShowsNoSearchResultsPage', async function() {
    await setFirstPartySkills({
      'Produce': [{id: '1', name: 'Apple'}],
    });

    page.onSearchChanged('Banana');
    await microtasksFinished();
    assertTrue(!!page.shadowRoot.querySelector('error-page'));

    page.onSearchChanged('');
    await microtasksFinished();
    assertFalse(!!page.shadowRoot.querySelector('error-page'));
  });
});
