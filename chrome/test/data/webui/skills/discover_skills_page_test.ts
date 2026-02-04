// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/discover_skills_page.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DiscoverSkillsPageElement} from 'chrome://skills/discover_skills_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DiscoverSkillsPage', function() {
  let page: DiscoverSkillsPageElement;

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    page = document.createElement('discover-skills-page');
    document.body.appendChild(page);
    return microtasksFinished();
  });

  test('InitialPageLoadsCorrectly', function() {
    const title = page.$['topPicksTitle'];
    assertTrue(!!title);
    assertEquals(
        loadTimeData.getString('topPicksTitle'), title.textContent.trim());

    const discoverTitle = page.$['discoverSkillsTitle'];
    assertTrue(!!discoverTitle);
    assertEquals(
        loadTimeData.getString('browseSkillsTitle'),
        discoverTitle.textContent.trim());
  });

  test('ChipClickTogglesIcon', async function() {
    const chips = page.shadowRoot.querySelectorAll('cr-chip');
    assertTrue(!!chips);
    assertTrue(chips.length >= 2);
    assertTrue(!!chips[0]);
    assertTrue(!!chips[1]);

    const firstChip = chips[0];
    const secondChip = chips[1];
    const firstIcon = firstChip.querySelector('cr-icon');
    assertTrue(!!firstIcon);
    const secondIcon = secondChip.querySelector('cr-icon');
    assertTrue(!!secondIcon);

    assertTrue(firstChip.selected);
    assertEquals('cr:check', firstIcon.icon);
    assertFalse(secondChip.selected);
    assertEquals('cr:add', secondIcon.icon);

    secondChip.click();
    await microtasksFinished();

    assertFalse(firstChip.selected);
    assertEquals('cr:add', firstIcon.icon);
    assertTrue(secondChip.selected);
    assertEquals('cr:check', secondIcon.icon);
  });
});
