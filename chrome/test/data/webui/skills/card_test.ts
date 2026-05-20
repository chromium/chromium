// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/card.js';

import type {SkillCardElement} from 'chrome://skills/card.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SkillCard', function() {
  let skillCard: SkillCardElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillCard = document.createElement('skill-card');
    skillCard.skill.name = 'Test Skill';
    skillCard.skill.icon = '🐶';
    skillCard.skill.prompt = 'Test prompt';
    document.body.appendChild(skillCard);
    return microtasksFinished();
  });

  test('SkillCardLoadsCorrectly', function() {
    assertEquals('Test Skill', skillCard.$.name.textContent.trim());
    assertEquals('🐶', skillCard.$.icon.textContent.trim());
    const cardBody = skillCard.shadowRoot.querySelector('#user-skill-cardBody');
    assertTrue(!!cardBody);
    assertEquals('Test prompt', cardBody.textContent.trim());
  });

  test('IllustrationUrlResizing', async function() {
    // Test with non-gstatic URL
    skillCard.skill = {
      ...skillCard.skill,
      imageUrl: 'https://example.com/image.png',
    };
    await microtasksFinished();
    let img = skillCard.shadowRoot.querySelector('#illustrationImage');
    assertTrue(!!img);
    assertEquals('https://example.com/image.png', img.getAttribute('auto-src'));

    // Test with gstatic URL
    skillCard.skill = {
      ...skillCard.skill,
      imageUrl: 'https://www.gstatic.com/chrome/skills/images/test.png',
    };
    await microtasksFinished();
    img = skillCard.shadowRoot.querySelector('#illustrationImage');
    assertTrue(!!img);
    const dpr = window.devicePixelRatio || 1;
    const expectedWidth = Math.round(238 * dpr);
    const expectedHeight = Math.round(119 * dpr);
    assertEquals(
        `https://www.gstatic.com/chrome/skills/images/test.png=w${
            expectedWidth}-h${expectedHeight}-rw`,
        img.getAttribute('auto-src'));
  });
});
