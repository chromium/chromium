// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/discover_skills_page.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DiscoverSkillsPageElement} from 'chrome://skills/discover_skills_page.js';
import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsPageBrowserProxy} from 'chrome://skills/skills_page_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('DiscoverSkillsPageLoadsCorrectly', async function() {
    const firstPartySkills = new Map<string, Skill[]>([
      [
        'Top Pick',
        [{
          id: '1',
          name: 'Top Skill',
          icon: '',
          prompt: '',
          source: SkillSource.kFirstParty,
          creationTime: {internalValue: 0n},
          lastUpdateTime: {internalValue: 0n},
        }],
      ],
      [
        'Writing',
        [{
          id: '2',
          name: 'Write',
          icon: '',
          prompt: '',
          source: SkillSource.kFirstParty,
          creationTime: {internalValue: 0n},
          lastUpdateTime: {internalValue: 0n},
        }],
      ],
    ]);
    browserProxy.callbackRouterRemote.update1PMap(
        Object.fromEntries(firstPartySkills));
    await microtasksFinished();
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
    browserProxy.callbackRouterRemote.update1PMap({});
    await microtasksFinished();
    // Discover skills title should still be there.
    const titles = page.shadowRoot.querySelectorAll('.page-title');
    assertEquals(1, titles.length);
    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertEquals(0, chips.length);
    const cards = page.shadowRoot.querySelectorAll('li');
    assertEquals(0, cards.length);
  });

  test('ChipClickTogglesIcon', async function() {
    const firstPartySkills = new Map<string, Skill[]>([
      [
        'Shopping',
        [{
          id: '1',
          name: 'Shopping',
          icon: '',
          prompt: '',
          source: SkillSource.kFirstParty,
          creationTime: {internalValue: 0n},
          lastUpdateTime: {internalValue: 0n},
        }],
      ],
      [
        'Writing',
        [{
          id: '2',
          name: 'Writing',
          icon: '',
          prompt: '',
          source: SkillSource.kFirstParty,
          creationTime: {internalValue: 0n},
          lastUpdateTime: {internalValue: 0n},
        }],
      ],
    ]);
    browserProxy.callbackRouterRemote.update1PMap(
        Object.fromEntries(firstPartySkills));
    await microtasksFinished();
    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertTrue(!!chips);
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
    assertEquals('cr:add', secondIcon.icon);

    chips[1].click();
    await microtasksFinished();

    assertFalse(chips[0].selected);
    assertEquals('cr:add', firstIcon.icon);
    assertTrue(chips[1].selected);
    assertEquals('cr:check', secondIcon.icon);
  });
});
