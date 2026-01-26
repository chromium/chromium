// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import type {PageHandlerInterface} from 'chrome://skills/skills.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'submitSkill',
      'closeDialog',
    ]);
  }

  submitSkill(skill: Skill) {
    this.methodCalled('submitSkill', skill);
  }

  closeDialog() {
    this.methodCalled('closeDialog');
  }
}

export class TestSkillsDialogBrowserProxy {
  handler: TestPageHandler;

  constructor() {
    this.handler = new TestPageHandler();
  }
}
