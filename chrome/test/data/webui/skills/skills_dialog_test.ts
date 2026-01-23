// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/skills_dialog_app.js';

import type {SkillsDialogAppElement} from 'chrome://skills/skills_dialog_app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SkillsDialogAppPage', function() {
  let skillsDialogApp: SkillsDialogAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsDialogApp = document.createElement('skills-dialog-app');
    document.body.appendChild(skillsDialogApp);
  });

  test('SkillsDialogAppLoads', function() {
    assertEquals(
        'Add Skill',
        skillsDialogApp.shadowRoot.querySelector('h1')!.textContent);
  });
});
