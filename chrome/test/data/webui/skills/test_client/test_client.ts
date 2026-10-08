// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, logMessage} from './client.js';
import {$} from './page_element_types.js';

function isDialogPath(path: string): boolean {
  return path.includes('/dialog');
}

function updateUrlDisplay() {
  const path = window.location.pathname;
  const search = window.location.search;
  const params = new URLSearchParams(search);

  if ($.currentPath) {
    $.currentPath.textContent = path;
  }

  const isDialog = isDialogPath(path);
  document.body.classList.toggle('dialog-mode', isDialog);
  if ($.dialogModeBadge) {
    $.dialogModeBadge.textContent =
        isDialog ? 'Dialog Mode (400px)' : 'Full Page Mode';
    $.dialogModeBadge.className =
        isDialog ? 'pill not-draggable dialog-active' : 'pill not-draggable';
  }

  if ($.dialogFormSection) {
    $.dialogFormSection.style.display = isDialog ? 'block' : 'none';
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
    if ($.dialogSkillNameInput) {
      $.dialogSkillNameInput.value = `Skill ${skillId}`;
    }
    if ($.dialogFormTitle) {
      $.dialogFormTitle.textContent = 'Edit skill';
    }
  } else if (params.get('isSavingGeminiPrompt') === 'true') {
    if ($.dialogFormTitle) {
      $.dialogFormTitle.textContent = 'Save Gemini Prompt';
    }
  } else {
    if ($.dialogFormTitle) {
      $.dialogFormTitle.textContent = 'Add skill';
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
  // Dialog Form Controls
  let isReviewMode = false;
  const setReviewMode = (review: boolean) => {
    isReviewMode = review;
    if ($.dialogStandardView) {
      $.dialogStandardView.style.display = isReviewMode ? 'none' : 'block';
    }
    if ($.dialogReviewView) {
      $.dialogReviewView.style.display = isReviewMode ? 'block' : 'none';
    }
    if ($.dialogFormTitle) {
      $.dialogFormTitle.textContent =
          isReviewMode ? 'Review before saving' : 'Add skill';
    }
    if ($.toggleReviewModeBtn) {
      $.toggleReviewModeBtn.textContent =
          isReviewMode ? 'Show Edit View' : 'Toggle "Review before saving"';
    }
    logMessage(`Switched dialog view to: ${
        isReviewMode ? 'Review before saving (compact)' :
                       'Standard Add/Edit'}`);
  };

  $.toggleReviewModeBtn?.addEventListener('click', () => {
    setReviewMode(!isReviewMode);
  });

  $.dialogReviewInstructionsBtn?.addEventListener('click', () => {
    setReviewMode(false);
  });

  $.dialogSaveBtn?.addEventListener('click', () => {
    client.showSaveToast();
  });

  $.dialogReviewSaveBtn?.addEventListener('click', () => {
    client.showSaveToast();
  });

  $.dialogCancelBtn?.addEventListener('click', () => {
    client.closeDialog();
  });

  $.dialogOpenFullPageBtn?.addEventListener('click', () => {
    const name = $.dialogSkillNameInput?.value || 'untitled-skill';
    const description = $.dialogSkillDescInput?.value || '';
    const instructions = $.dialogSkillInstructionsInput?.value || '';
    client.openFullPageEditor({
      url: '/chromeskills/editor',
      name,
      description,
      instructions,
      icon: '😊',
    });
  });

  // Width Presets
  const setBodyWidth = (width: string) => {
    document.body.style.width = width;
    document.body.style.maxWidth = width;
    if ($.pageHeader) {
      $.pageHeader.style.width = width;
      $.pageHeader.style.maxWidth = width;
    }
    const contentEl = document.getElementById('content');
    if (contentEl) {
      contentEl.style.width = width;
      contentEl.style.maxWidth = width;
    }
    logMessage(`Set client width to: ${width}`);
  };

  $.dialogWidth400Btn?.addEventListener('click', () => {
    setBodyWidth('400px');
  });

  $.dialogWidth380Btn?.addEventListener('click', () => {
    setBodyWidth('380px');
  });

  $.dialogWidth512Btn?.addEventListener('click', () => {
    setBodyWidth('512px');
  });

  $.dialogWidthFullBtn?.addEventListener('click', () => {
    document.body.style.width = '';
    document.body.style.maxWidth = '';
    if ($.pageHeader) {
      $.pageHeader.style.width = '';
      $.pageHeader.style.maxWidth = '';
    }
    const contentEl = document.getElementById('content');
    if (contentEl) {
      contentEl.style.width = '';
      contentEl.style.maxWidth = '';
    }
    logMessage('Reset client width to 100% full width.');
  });

  $.toggleDialogModeBtn?.addEventListener('click', () => {
    const active = document.body.classList.toggle('dialog-mode');
    if ($.dialogModeBadge) {
      $.dialogModeBadge.textContent =
          active ? 'Dialog Mode (400px)' : 'Full Page Mode';
      $.dialogModeBadge.className =
          active ? 'pill not-draggable dialog-active' : 'pill not-draggable';
    }
    if ($.dialogFormSection) {
      $.dialogFormSection.style.display = active ? 'block' : 'none';
    }
    logMessage(`Toggled dialog-mode: ${active}`);
  });

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

function updateViewportMetrics() {
  const winW = window.innerWidth;
  const winH = window.innerHeight;
  const dpr = window.devicePixelRatio;
  const doc = document.documentElement;
  const body = document.body;

  const wEl = document.getElementById('metricWindowSize');
  if (wEl) {
    wEl.textContent = `${winW} × ${winH} (outer: ${window.outerWidth} × ${
        window.outerHeight})`;
  }

  const dprEl = document.getElementById('metricDPR');
  if (dprEl) {
    dprEl.textContent = `${dpr}`;
  }

  const hcEl = document.getElementById('metricHtmlClient');
  if (hcEl && doc) {
    hcEl.textContent = `${doc.clientWidth} × ${doc.clientHeight}`;
  }

  const hsEl = document.getElementById('metricHtmlScroll');
  if (hsEl && doc) {
    hsEl.textContent = `${doc.scrollWidth} × ${doc.scrollHeight}`;
  }

  const hoEl = document.getElementById('metricHtmlOffset');
  if (hoEl && doc) {
    hoEl.textContent = `${doc.offsetWidth} × ${doc.offsetHeight}`;
  }

  const bcEl = document.getElementById('metricBodyClient');
  if (bcEl && body) {
    bcEl.textContent = `${body.clientWidth} × ${body.clientHeight}`;
  }

  const bsEl = document.getElementById('metricBodyScroll');
  if (bsEl && body) {
    bsEl.textContent = `${body.scrollWidth} × ${body.scrollHeight}`;
  }

  const boEl = document.getElementById('metricBodyOffset');
  if (boEl && body) {
    boEl.textContent = `${body.offsetWidth} × ${body.offsetHeight}`;
  }

  const scEl = document.getElementById('metricScreenSize');
  if (scEl) {
    scEl.textContent = `${screen.width} × ${screen.height} (avail: ${
        screen.availWidth} × ${screen.availHeight})`;
  }

  const vvEl = document.getElementById('metricVisualViewport');
  if (vvEl && window.visualViewport) {
    vvEl.textContent = `${Math.round(window.visualViewport.width)} × ${
        Math.round(window.visualViewport.height)}`;
  }
}

// Initialize test client
function init() {
  client.init();
  initEventListeners();
  updateUrlDisplay();
  updateViewportMetrics();

  window.addEventListener('resize', updateViewportMetrics);
  window.addEventListener('scroll', updateViewportMetrics);
  if (window.visualViewport) {
    window.visualViewport.addEventListener('resize', updateViewportMetrics);
    window.visualViewport.addEventListener('scroll', updateViewportMetrics);
  }
  setInterval(updateViewportMetrics, 500);
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
