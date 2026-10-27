// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerRemote} from 'chrome://skills/skills.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export function createTestSkillsPageHandler(): TestMock<PageHandlerRemote>&
    PageHandlerRemote {
  const handler = TestMock.fromClass(PageHandlerRemote);
  handler.setResultFor('getInitialUserSkills', Promise.resolve({skills: []}));
  handler.setResultFor(
      'getInitial1PSkills',
      Promise.resolve({initialState: {topicsList: [], skillMap: {}}}));
  handler.setResultFor('maybeSave1PSkill', Promise.resolve({success: true}));
  handler.setResultFor('recordSkillsManagementAction', Promise.resolve());
  return handler;
}
