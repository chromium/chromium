// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/user_skills_page.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import type {UserSkillsPageElement} from 'chrome://skills/user_skills_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UserSkillsPage', function() {
  let page: UserSkillsPageElement;

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    page = document.createElement('user-skills-page');
    document.body.appendChild(page);
    return microtasksFinished();
  });

  test('InitialPageLoadsCorrectly', function() {
    const title = page.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals('Your skills', title.textContent.trim());

    const emptyState = page.shadowRoot.querySelector('#empty-state');
    assertTrue(!!emptyState);
    const notice = emptyState.querySelector('#notice-message');
    assertTrue(!!notice);
    assertEquals(
        'Skills help simplify and automate repetitive tasks',
        notice.textContent.trim());
  });
});
