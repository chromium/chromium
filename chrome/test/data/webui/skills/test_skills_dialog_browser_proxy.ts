// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Skill} from 'chrome://skills/skill.mojom-webui.js';
import {SkillSource} from 'chrome://skills/skill.mojom-webui.js';
import type {DialogHandlerInterface} from 'chrome://skills/skills.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDialogHandler extends TestBrowserProxy implements
    DialogHandlerInterface {
  private initialSkill_: Skill = {
    id: '',
    name: '',
    icon: '',
    prompt: '',
    source: SkillSource.kUnknown,
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };

  constructor() {
    super([
      'submitSkill',
      'refineSkill',
      'closeDialog',
      'showEmojiPicker',
      'getInitialSkill',
    ]);
  }

  submitSkill(skill: Skill) {
    this.methodCalled('submitSkill', skill);
  }

  refineSkill(skill: Skill) {
    this.methodCalled('refineSkill', skill);
    // Return a default successful promise to satisfy the interface
    return Promise.resolve({refinedSkill: skill});
  }

  closeDialog() {
    this.methodCalled('closeDialog');
  }

  showEmojiPicker() {
    this.methodCalled('showEmojiPicker');
  }

  getInitialSkill() {
    this.methodCalled('getInitialSkill');
    return Promise.resolve({skill: this.initialSkill_});
  }

  setInitialSkill(skill: Skill) {
    this.initialSkill_ = skill;
  }
}

export class TestSkillsDialogBrowserProxy {
  handler: TestDialogHandler;

  constructor() {
    this.handler = new TestDialogHandler();
  }
}
