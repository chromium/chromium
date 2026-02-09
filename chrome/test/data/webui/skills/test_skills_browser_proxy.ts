// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerRemote, SkillsPageCallbackRouter} from 'chrome://skills/skills.mojom-webui.js';
import type {SkillsPageRemote} from 'chrome://skills/skills.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestSkillsBrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: SkillsPageCallbackRouter;
  callbackRouterRemote: SkillsPageRemote;

  constructor() {
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new SkillsPageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler.setResultFor(
        'getInitialUserSkills', Promise.resolve({skills: []}));
    this.handler.setResultFor(
        'getInitial1PSkills', Promise.resolve({skillMap: {}}));
    this.handler.setResultFor(
        'maybeSave1PSkill', Promise.resolve({success: true}));
  }
}
