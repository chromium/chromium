// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SkillPreview, SkillSource} from '/glic/glic_api/glic_api.js';

import {client, getBrowser, logMessage} from '../client.js';
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

function updateSkillsListUi(skillPreviews: SkillPreview[]) {
  while ($.skillsList.childNodes.length > 0) {
    $.skillsList.removeChild($.skillsList.firstChild!);
  }

  skillPreviews.forEach((skill: SkillPreview) => {
    const li = document.createElement('LI');

    // ID.
    const id = document.createElement('SPAN');
    id.className = 'skill-id';
    id.setAttribute('value', skill.id);
    id.innerText = skill.id;
    li.appendChild(id);

    // Name.
    const name = document.createElement('SPAN');
    name.innerText = skill.name;
    name.className = 'skill-name';
    name.setAttribute('value', skill.name);
    li.appendChild(name);

    $.skillsList.appendChild(li);
  });
}

function initSkillPreviews() {
  const browser = getBrowser()!;
  if (browser.getSkillPreviews) {
    const observableSkillPreviews = browser.getSkillPreviews()!;
    observableSkillPreviews.subscribeObserver!({
      next: (skillPreviews: SkillPreview[]) => {
        logMessage(`skills previews updated.`);
        updateSkillsListUi(skillPreviews);
      },
      error: (err: any) => {
        logMessage(`skill previews update error: ${err}`);
      },
    });
  } else {
    logMessage('getSkillPreviews not supported');
  }
}

client.getInitialized().then(initSkillPreviews);
