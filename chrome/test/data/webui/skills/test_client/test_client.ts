// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, logMessage} from './client.js';
import {$} from './page_element_types.js';

function updateUrlDisplay() {
  const path = window.location.pathname;
  const search = window.location.search;
  const params = new URLSearchParams(search);

  if ($.currentPath) {
    $.currentPath.textContent = path;
  }

  if ($.queryParamsDisplay) {
    const paramEntries: string[] = [];
    params.forEach((val, key) => {
      paramEntries.push(`${key}=${val}`);
    });
    $.queryParamsDisplay.textContent =
        paramEntries.length > 0 ? `?${paramEntries.join('&')}` : '(none)';
  }

  // If query params specify a skill or prompt, populate fields.
  if (params.has('id')) {
    const skillId = params.get('id')!;
    if ($.toastSkillIdInput) {
      $.toastSkillIdInput.value = skillId;
    }
    if ($.deleteToastSkillIdInput) {
      $.deleteToastSkillIdInput.value = skillId;
    }
    if ($.invokeSkillIdInput) {
      $.invokeSkillIdInput.value = skillId;
    }
  }

  logMessage(`Loaded page at path: "${path}", query: "${search}"`);
}

function navigateTo(pathWithQuery: string) {
  window.history.pushState({}, '', pathWithQuery);
  updateUrlDisplay();
  logMessage(`Navigated to: ${pathWithQuery}`);
}

function initEventListeners() {
  // Header / Status
  $.refreshBtn?.addEventListener('click', () => {
    location.reload();
  });

  $.closeDialogHeaderBtn?.addEventListener('click', () => {
    client.closeDialog();
  });

  // Received From Host Copy buttons
  $.copyGeminiPromptBtn?.addEventListener('click', () => {
    if ($.geminiPromptReceived && $.promptInput) {
      $.promptInput.value = $.geminiPromptReceived.value;
      logMessage('Copied received Gemini prompt to Send Prompt input.');
    }
  });

  $.copyDialogInfoBtn?.addEventListener('click', () => {
    if (client.receivedDialogInfo) {
      const info = client.receivedDialogInfo;
      if ($.editorSkillNameInput && info.skillName) {
        $.editorSkillNameInput.value = info.skillName;
      }
      if ($.editorSkillDescInput && info.skillDescription) {
        $.editorSkillDescInput.value = info.skillDescription;
      }
      if ($.editorSkillInstructionsInput && info.skillInstructions) {
        $.editorSkillInstructionsInput.value = info.skillInstructions;
      }
      if ($.editorSkillIconInput && info.skillIcon) {
        $.editorSkillIconInput.value = info.skillIcon;
      }
      logMessage('Copied dialog info to Full Page Editor inputs.');
    }
  });

  // Provided Skills
  $.getProvidedSkillBtn?.addEventListener('click', () => {
    const skillId = $.getProvidedSkillIdInput?.value?.trim();
    if (skillId) {
      client.getProvidedSkill(skillId);
    }
  });

  $.copyProvidedSkillPromptBtn?.addEventListener('click', () => {
    if ($.providedSkillPrompt && $.promptInput) {
      $.promptInput.value = $.providedSkillPrompt.value;
      logMessage('Copied provided skill prompt to Send Prompt input.');
    }
  });

  // Toasts
  $.showSaveToastBtn?.addEventListener('click', () => {
    client.showSaveToast();
  });

  $.showSaveAndInvokeToastBtn?.addEventListener('click', () => {
    const skillId = $.toastSkillIdInput?.value || 'skill_123';
    const skillName = $.toastSkillNameInput?.value || 'Summarize Page';
    const skillIcon = $.toastSkillIconInput?.value || '✨';
    client.showSaveAndInvokeToast(skillId, skillName, skillIcon);
  });

  $.showDeleteToastBtn?.addEventListener('click', () => {
    const skillId = $.deleteToastSkillIdInput?.value || 'skill_123';
    client.showDeleteToast(skillId);
  });

  // Skill Invocation
  $.invokeSkillBtn?.addEventListener('click', () => {
    const skillId = $.invokeSkillIdInput?.value || 'skill_123';
    const skillName = $.invokeSkillNameInput?.value || 'Summarize Page';
    const skillIcon = $.invokeSkillIconInput?.value || '✨';
    client.invokeSkill(skillId, skillName, skillIcon);
  });

  // Send Prompt
  $.sendPromptBtn?.addEventListener('click', () => {
    const prompt = $.promptInput?.value || 'Summarize this page in 3 bullets.';
    client.sendPrompt(prompt);
  });

  // Full Page Editor
  $.openEditorBtn?.addEventListener('click', () => {
    const url = $.editorUrlInput?.value || '/chromeskills/yourSkills';
    const name = $.editorSkillNameInput?.value || 'My Custom Skill';
    const description =
        $.editorSkillDescInput?.value || 'A helpful skill for research.';
    const instructions = $.editorSkillInstructionsInput?.value ||
        'Please analyze the main points.';
    const icon = $.editorSkillIconInput?.value || '🚀';

    client.openFullPageEditor({
      url,
      name,
      description,
      instructions,
      icon,
    });
  });

  // Open URL
  $.openUrlBtn?.addEventListener('click', () => {
    const url = $.openUrlInput?.value || 'https://www.google.com';
    client.openUrl(url);
  });

  // Close Dialog
  $.closeDialogBtn?.addEventListener('click', () => {
    client.closeDialog();
  });

  // Performance Metrics
  $.sendFrameworkMetricBtn?.addEventListener('click', () => {
    const value = parseFloat($.metricFrameworkLoadTime?.value || '120');
    client.logMetric('framework-load-time', value);
  });

  $.sendWebClientMetricBtn?.addEventListener('click', () => {
    const value = parseFloat($.metricWebClientLoadTime?.value || '250');
    client.logMetric('web-client-load-time', value);
  });

  $.sendDataFetchMetricBtn?.addEventListener('click', () => {
    const value = parseFloat($.metricDataFetchTime?.value || '300');
    client.logMetric('guest-data-fetch-time', value);
  });

  $.sendDataSaveMetricBtn?.addEventListener('click', () => {
    const value = parseFloat($.metricDataSaveTime?.value || '450');
    client.logMetric('guest-data-save-time', value);
  });

  $.sendAllMetricsBtn?.addEventListener('click', () => {
    const fVal = parseFloat($.metricFrameworkLoadTime?.value || '120');
    const wVal = parseFloat($.metricWebClientLoadTime?.value || '250');
    const dVal = parseFloat($.metricDataFetchTime?.value || '300');
    const sVal = parseFloat($.metricDataSaveTime?.value || '450');

    client.logMetric('framework-load-time', fVal);
    client.logMetric('web-client-load-time', wVal);
    client.logMetric('guest-data-fetch-time', dVal);
    client.logMetric('guest-data-save-time', sVal);
    logMessage('Logged all 4 performance metrics.');
  });

  // Navigation Simulation
  $.navBrowseBtn?.addEventListener('click', () => {
    navigateTo('/chromeskills/browse');
  });

  $.navYourSkillsBtn?.addEventListener('click', () => {
    navigateTo('/chromeskills/yourSkills');
  });

  $.navAddDialogBtn?.addEventListener('click', () => {
    navigateTo('/chromeskills/dialog?source=user');
  });

  $.navEditDialogBtn?.addEventListener('click', () => {
    navigateTo('/chromeskills/dialog?id=sample_skill_123&source=user');
  });

  $.navEditorBtn?.addEventListener('click', () => {
    navigateTo('/chromeskills/editor');
  });

  $.customNavBtn?.addEventListener('click', () => {
    const path = $.customNavPathInput?.value || '/chromeskills/browse';
    navigateTo(path);
  });

  // Logs
  $.clearLogBtn?.addEventListener('click', () => {
    if ($.eventLog) {
      $.eventLog.innerHTML = '';
    }
  });

  window.addEventListener('popstate', () => {
    updateUrlDisplay();
  });
}

// Initialize test client
function init() {
  client.init();
  initEventListeners();
  updateUrlDisplay();
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
