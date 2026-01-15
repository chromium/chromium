// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/app.js';

import type {SkillsAppElement} from 'chrome://skills/app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SkillsAppPage', function() {
  let skillsApp: SkillsAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    skillsApp = document.createElement('skills-app');
    document.body.appendChild(skillsApp);
  });

  test('SkillsPageLoads', function() {
    assertEquals(
        'Skills', skillsApp.shadowRoot.querySelector('h1')!.textContent);
  });
});
