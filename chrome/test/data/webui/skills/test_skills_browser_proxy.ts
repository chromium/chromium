// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import type {PageHandlerInterface, SkillsDialogType} from 'chrome://skills/skills.mojom-webui.js';
import {SkillsPageCallbackRouter} from 'chrome://skills/skills.mojom-webui.js';
import type {SkillsPageRemote} from 'chrome://skills/skills.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'openSkillsDialog',
    ]);
  }

  openSkillsDialog(dialogType: SkillsDialogType, skill: Skill|null) {
    this.methodCalled('openSkillsDialog', dialogType, skill);
  }
}

export class TestSkillsBrowserProxy {
  handler: TestPageHandler;
  callbackRouter: SkillsPageCallbackRouter = new SkillsPageCallbackRouter();
  remote: SkillsPageRemote;

  constructor() {
    this.handler = new TestPageHandler();
    this.remote = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}
