// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SkillSource} from '/glic/glic_api/glic_api.js';

import {getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

$.createSkillBtn.addEventListener('click', async () => {
  const id = $.skillIdInput.value;
  const name = $.skillNameInput.value;
  const icon = $.skillIconInput.value;
  const prompt = $.skillPromptInput.value;
  const source = Number($.skillSourceSelect.value) as SkillSource;
  try {
    await getBrowser()!.createSkill!({id, name, icon, prompt, source});
    logMessage('createSkill called');
  } catch (e) {
    logMessage(`createSkill failed: ${e}`);
  }
});

$.updateSkillBtn.addEventListener('click', async () => {
  const id = $.skillIdInput.value;
  if (!id) {
    logMessage('Skill ID is required for update');
    return;
  }
  try {
    await getBrowser()!.updateSkill!({id});
    logMessage('updateSkill called');
  } catch (e) {
    logMessage(`updateSkill failed: ${e}`);
  }
});

$.getSkillBtn.addEventListener('click', async () => {
  const id = $.skillIdInput.value;
  if (!id) {
    logMessage('Skill ID is required for get');
    return;
  }
  try {
    const skill = await getBrowser()!.getSkill!(id);
    logMessage(`getSkill returned: ${JSON.stringify(skill)}`);
  } catch (e) {
    logMessage(`getSkill failed: ${e}`);
  }
});

$.manageSkillsBtn.addEventListener('click', () => {
  getBrowser()!.showManageSkillsUi!();
});
