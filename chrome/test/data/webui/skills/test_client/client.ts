// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from './page_element_types.js';

// Protocol message types matching
// chrome://skills/v2/skills_webview_bridge_constants.js
export const SKILLS_HANDSHAKE_TYPE = 'skills-handshake';
export const SKILLS_HANDSHAKE_ACK = 'SKILLS_HANDSHAKE_ACK';
export const SKILLS_SHOW_TOAST = 'show-toast';
export const SKILLS_INVOKE_SKILL = 'invoke-skill';
export const SKILLS_CLOSE_DIALOG = 'close-dialog';
export const SKILLS_OPEN_URL = 'open-url';
export const SKILLS_SEND_PROMPT = 'send-prompt';
export const SKILLS_OPEN_FULL_PAGE_EDITOR = 'open-full-page-editor';
export const SKILLS_DIALOG_INFO_TYPE = 'skills-dialog-info';
export const SKILLS_GEMINI_PROMPT_TYPE = 'skills-gemini-prompt';
export const SKILLS_UNDO_TYPE = 'skills-undo';
export const SKILLS_TOAST_CLOSED_TYPE = 'toast-closed';
export const SKILLS_LOG_METRIC = 'log-metric';
export const SKILLS_SEND_PROVIDED_SKILLS_TYPE = 'send-provided-skills';
export const SKILLS_GET_PROVIDED_SKILL = 'get-provided-skill';
export const SKILLS_PROVIDED_SKILL_INFO_TYPE = 'provided-skill-info';
export const IS_SAVING_GEMINI_QUERY_PARAMETER = 'isSavingGeminiPrompt';
export const SOURCE_QUERY_PARAMETER = 'source';

export enum SkillSource {
  FIRST_PARTY = 'first-party',
  USER = 'user',
  PROVIDED = 'provided',
}

export interface SkillPreview {
  id: string;
  name: string;
  icon?: string;
  imageUrl?: string;
  description?: string;
  category?: string;
}

export interface Skill {
  id: string;
  sourceSkillId?: string|null;
  name: string;
  icon?: string;
  imageUrl?: string;
  prompt?: string;
  description?: string;
  curatedBy?: string;
  source?: number;
  creationTime?: {internalValue: bigint|number|string};
  lastUpdateTime?: {internalValue: bigint|number|string};
  category?: string;
}

export interface SkillDialogInfo {
  skillIcon?: string;
  skillName?: string;
  skillDescription?: string;
  skillInstructions?: string;
}

export interface PendingEditorData {
  name: string;
  description: string;
  instructions: string;
  icon: string;
  url: string;
}

export function logMessage(message: string) {
  const d = new Date();
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  const ss = String(d.getSeconds()).padStart(2, '0');
  const ms = String(d.getMilliseconds()).padStart(3, '0');
  const timeStamp = `${hh}:${mm}:${ss}.${ms}: `;

  if ($.eventLog) {
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.textContent = `${timeStamp}${message}`;
    $.eventLog.prepend(entry);
  }

  if ($.status) {
    $.status.textContent = `${timeStamp}${message}`;
  }

  console.info(`[SkillsTestClient] ${timeStamp}${message}`);
}

export class SkillsTestClient {
  private hostWindow: WindowProxy|null = null;
  private hostOrigin: string = '';
  private isConnected: boolean = false;
  private onConnectedCallbacks: Array<() => void> = [];
  private initialized: boolean = false;
  receivedPrompt: string = '';
  receivedDialogInfo: SkillDialogInfo|null = null;
  receivedProvidedSkills: SkillPreview[] = [];
  receivedProvidedSkillInfo: Skill|null = null;

  constructor() {
    this.init();
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;
    logMessage('Skills test client initialized. Waiting for handshake...');
    window.addEventListener('message', (e: MessageEvent) => this.onMessage(e));
  }

  private onMessage(event: MessageEvent) {
    if (!event.data) {
      return;
    }

    const msg = event.data;

    // Handle handshake ping.
    if (msg.type === SKILLS_HANDSHAKE_TYPE) {
      this.hostWindow = (event.source as WindowProxy) || window.parent;
      this.hostOrigin = event.origin;
      this.isConnected = true;

      logMessage(
          `Received handshake ping from ${event.origin}. Sending ACK...`);
      this.sendHandshakeAck();

      if ($.pageHeader) {
        $.pageHeader.classList.add('connected');
      }
      if ($.connectionStatus) {
        $.connectionStatus.textContent = `Connected to ${event.origin}`;
        $.connectionStatus.className = 'status-badge connected';
      }

      const callbacks = this.onConnectedCallbacks;
      this.onConnectedCallbacks = [];
      for (const cb of callbacks) {
        cb();
      }
      return;
    }

    // Handle incoming prompt from host (e.g. Save Prompt as Skill).
    if (msg.type === SKILLS_GEMINI_PROMPT_TYPE) {
      this.receivedPrompt = msg.prompt ?? '';
      logMessage(`Received Gemini prompt: "${this.receivedPrompt}"`);
      if ($.geminiPromptReceived) {
        $.geminiPromptReceived.value = this.receivedPrompt;
      }
      if ($.promptInput) {
        $.promptInput.value = this.receivedPrompt;
      }
      return;
    }

    // Handle skill dialog info (opening editor with pending data).
    if (msg.type === SKILLS_DIALOG_INFO_TYPE) {
      this.receivedDialogInfo = {
        skillIcon: msg.skillIcon,
        skillName: msg.skillName,
        skillDescription: msg.skillDescription,
        skillInstructions: msg.skillInstructions,
      };
      logMessage(`Received Skill Dialog Info: name="${msg.skillName}", icon="${
          msg.skillIcon}"`);
      if ($.dialogInfoIcon) {
        $.dialogInfoIcon.textContent = msg.skillIcon ?? '';
      }
      if ($.dialogInfoName) {
        $.dialogInfoName.textContent = msg.skillName ?? '';
      }
      if ($.dialogInfoDescription) {
        $.dialogInfoDescription.textContent = msg.skillDescription ?? '';
      }
      if ($.dialogInfoInstructions) {
        $.dialogInfoInstructions.textContent = msg.skillInstructions ?? '';
      }

      // Pre-fill editor inputs.
      if ($.editorSkillNameInput && msg.skillName) {
        $.editorSkillNameInput.value = msg.skillName;
      }
      if ($.editorSkillDescInput && msg.skillDescription) {
        $.editorSkillDescInput.value = msg.skillDescription;
      }
      if ($.editorSkillInstructionsInput && msg.skillInstructions) {
        $.editorSkillInstructionsInput.value = msg.skillInstructions;
      }
      if ($.editorSkillIconInput && msg.skillIcon) {
        $.editorSkillIconInput.value = msg.skillIcon;
      }
      return;
    }

    // Handle undo notification.
    if (msg.type === SKILLS_UNDO_TYPE) {
      logMessage(`Received Undo notification for skill ID: "${msg.skillId}"`);
      if ($.undoStatus) {
        $.undoStatus.textContent = `Undo clicked for skillId: ${msg.skillId}`;
      }
      return;
    }

    // Handle toast closed notification.
    if (msg.type === SKILLS_TOAST_CLOSED_TYPE) {
      logMessage(
          `Received Toast Closed notification for skill ID: "${msg.skillId}"`);
      if ($.toastClosedStatus) {
        $.toastClosedStatus.textContent =
            `Toast closed for skillId: ${msg.skillId}`;
      }
      return;
    }

    // Handle provided skills list from host.
    if (msg.type === SKILLS_SEND_PROVIDED_SKILLS_TYPE) {
      this.receivedProvidedSkills = msg.payload ?? [];
      logMessage(
          `Received ${this.receivedProvidedSkills.length} Provided Skills`);
      this.updateProvidedSkillsUi();
      return;
    }

    // Handle provided skill details from host.
    if (msg.type === SKILLS_PROVIDED_SKILL_INFO_TYPE) {
      this.receivedProvidedSkillInfo = msg.payload ?? null;
      if (this.receivedProvidedSkillInfo) {
        logMessage(`Received Provided Skill Info: ID="${
            this.receivedProvidedSkillInfo.id}", Name="${
            this.receivedProvidedSkillInfo.name}"`);
      } else {
        logMessage('Received Provided Skill Info: null (not found)');
      }
      this.updateProvidedSkillInfoUi();
      return;
    }

    logMessage(`Received unhandled message: ${JSON.stringify(msg)}`);
  }

  private updateProvidedSkillsUi() {
    if ($.providedSkillsCount) {
      $.providedSkillsCount.textContent =
          `(${this.receivedProvidedSkills.length} skills)`;
    }
    if ($.providedSkillsList) {
      $.providedSkillsList.innerHTML = '';
      if (this.receivedProvidedSkills.length === 0) {
        const empty = document.createElement('div');
        empty.textContent = 'No provided skills available.';
        empty.style.color = 'var(--subtext-color)';
        empty.style.padding = '8px';
        $.providedSkillsList.appendChild(empty);
      } else {
        for (const skill of this.receivedProvidedSkills) {
          const card = document.createElement('div');
          card.className = 'info-box';
          card.style.display = 'flex';
          card.style.justifyContent = 'space-between';
          card.style.alignItems = 'center';
          card.style.marginBottom = '6px';
          card.innerHTML = `
            <div>
              <span style="font-size: 14px; margin-right: 6px;">${
              skill.icon || '📄'}</span>
              <strong>${skill.name}</strong>
              <span class="pill" style="margin-left: 6px;">ID: ${
              skill.id}</span>
              ${
              skill.category ? `<span class="pill" style="margin-left: 4px;">${
                                   skill.category}</span>` :
                               ''}
              ${
              skill.description ?
                  ('<div style="color: var(--subtext-color); margin-top: 4px;' +
                   `font-size: 11px;">${skill.description}</div>`) :
                  ''}
            </div>
            <button class="small secondary fetch-btn" data-id="${
              skill.id}">Fetch Details</button>
          `;
          card.querySelector('.fetch-btn')?.addEventListener('click', () => {
            if ($.getProvidedSkillIdInput) {
              $.getProvidedSkillIdInput.value = skill.id;
            }
            this.getProvidedSkill(skill.id);
          });
          $.providedSkillsList.appendChild(card);
        }
      }
    }
    if ($.getProvidedSkillIdInput && this.receivedProvidedSkills.length > 0 &&
        (!$.getProvidedSkillIdInput.value ||
         $.getProvidedSkillIdInput.value === 'skill_sample_01')) {
      $.getProvidedSkillIdInput.value =
          this.receivedProvidedSkills[0]?.id || '';
    }
  }

  private updateProvidedSkillInfoUi() {
    const skill = this.receivedProvidedSkillInfo;
    if (!skill) {
      if ($.providedSkillDetailId) {
        $.providedSkillDetailId.textContent = '(not found)';
      }
      if ($.providedSkillDetailName) {
        $.providedSkillDetailName.textContent = '(not found)';
      }
      if ($.providedSkillDetailIcon) {
        $.providedSkillDetailIcon.textContent = '(none)';
      }
      if ($.providedSkillDetailCategory) {
        $.providedSkillDetailCategory.textContent = '(none)';
      }
      if ($.providedSkillDetailDescription) {
        $.providedSkillDetailDescription.textContent = '(none)';
      }
      if ($.providedSkillDetailCuratedBy) {
        $.providedSkillDetailCuratedBy.textContent = '(none)';
      }
      if ($.providedSkillPrompt) {
        $.providedSkillPrompt.value = '';
      }
      return;
    }

    if ($.providedSkillDetailId) {
      $.providedSkillDetailId.textContent = skill.id || '(none)';
    }
    if ($.providedSkillDetailName) {
      $.providedSkillDetailName.textContent = skill.name || '(none)';
    }
    if ($.providedSkillDetailIcon) {
      $.providedSkillDetailIcon.textContent = skill.icon || '(none)';
    }
    if ($.providedSkillDetailCategory) {
      $.providedSkillDetailCategory.textContent = skill.category || '(none)';
    }
    if ($.providedSkillDetailDescription) {
      $.providedSkillDetailDescription.textContent =
          skill.description || '(none)';
    }
    if ($.providedSkillDetailCuratedBy) {
      $.providedSkillDetailCuratedBy.textContent = skill.curatedBy || '(none)';
    }
    if ($.providedSkillPrompt) {
      $.providedSkillPrompt.value = skill.prompt || '';
    }
  }

  postMessageToHost(message: Record<string, any>) {
    const target = this.hostWindow || window.parent;
    const origin = this.hostOrigin || '*';
    target.postMessage(message, origin);
    logMessage(`Sent message: ${JSON.stringify(message)}`);
  }

  sendHandshakeAck() {
    this.postMessageToHost({type: SKILLS_HANDSHAKE_ACK});
  }

  showSaveToast() {
    this.postMessageToHost({
      type: SKILLS_SHOW_TOAST,
      toastType: 'save',
    });
  }

  showSaveAndInvokeToast(
      skillId: string, skillName: string, skillIcon: string) {
    this.postMessageToHost({
      type: SKILLS_SHOW_TOAST,
      toastType: 'save_and_invoke',
      skillId,
      skillName,
      skillIcon,
    });
  }

  showDeleteToast(skillId: string) {
    this.postMessageToHost({
      type: SKILLS_SHOW_TOAST,
      toastType: 'delete',
      skillId,
    });
  }

  invokeSkill(skillId: string, skillName: string, skillIcon: string) {
    this.postMessageToHost({
      type: SKILLS_INVOKE_SKILL,
      skillId,
      skillName,
      skillIcon,
    });
  }

  closeDialog() {
    this.postMessageToHost({
      type: SKILLS_CLOSE_DIALOG,
    });
  }

  openFullPageEditor(data: PendingEditorData) {
    this.postMessageToHost({
      type: SKILLS_OPEN_FULL_PAGE_EDITOR,
      url: data.url,
      skillName: data.name,
      skillDescription: data.description,
      skillInstructions: data.instructions,
      skillIcon: data.icon,
    });
  }

  openUrl(url: string) {
    this.postMessageToHost({
      type: SKILLS_OPEN_URL,
      url,
    });
  }

  sendPrompt(prompt: string) {
    this.postMessageToHost({
      type: SKILLS_SEND_PROMPT,
      prompt,
    });
  }

  logMetric(metricName: string, valueMs: number) {
    this.postMessageToHost({
      type: SKILLS_LOG_METRIC,
      metricName,
      valueMs,
    });
  }

  getProvidedSkill(skillId: string) {
    this.postMessageToHost({
      type: SKILLS_GET_PROVIDED_SKILL,
      skillId,
    });
  }

  getReceivedProvidedSkills(): SkillPreview[] {
    return this.receivedProvidedSkills;
  }

  getReceivedProvidedSkillInfo(): Skill|null {
    return this.receivedProvidedSkillInfo;
  }

  whenConnected(): Promise<void> {
    return new Promise<void>((resolve) => {
      if (this.isConnected) {
        resolve();
        return;
      }
      this.onConnectedCallbacks.push(resolve);
    });
  }

  getConnected(): boolean {
    return this.isConnected;
  }
}

export const client = new SkillsTestClient();

// Allow automated tests to access the test client on window.
declare global {
  interface Window {
    client: SkillsTestClient;
    skillsTestClient: SkillsTestClient;
  }
}

window.client = client;
window.skillsTestClient = client;
