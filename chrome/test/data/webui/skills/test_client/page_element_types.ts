// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PageElementTypes {
  // Header / Status
  pageHeader: HTMLElement;
  connectionStatus: HTMLElement;
  currentPath: HTMLSpanElement;
  queryParamsDisplay: HTMLSpanElement;
  refreshBtn: HTMLButtonElement;
  closeDialogHeaderBtn: HTMLButtonElement;

  // Received From Host
  geminiPromptReceived: HTMLTextAreaElement;
  copyGeminiPromptBtn: HTMLButtonElement;
  dialogInfoIcon: HTMLSpanElement;
  dialogInfoName: HTMLSpanElement;
  dialogInfoDescription: HTMLSpanElement;
  dialogInfoInstructions: HTMLSpanElement;
  copyDialogInfoBtn: HTMLButtonElement;
  undoStatus: HTMLSpanElement;
  toastClosedStatus: HTMLSpanElement;
  providedSkillsCount: HTMLSpanElement;
  providedSkillsList: HTMLDivElement;
  getProvidedSkillIdInput: HTMLInputElement;
  getProvidedSkillBtn: HTMLButtonElement;
  providedSkillDetailInfo: HTMLDivElement;
  providedSkillDetailId: HTMLSpanElement;
  providedSkillDetailName: HTMLSpanElement;
  providedSkillDetailIcon: HTMLSpanElement;
  providedSkillDetailCategory: HTMLSpanElement;
  providedSkillDetailDescription: HTMLSpanElement;
  providedSkillDetailCuratedBy: HTMLSpanElement;
  providedSkillPrompt: HTMLTextAreaElement;
  copyProvidedSkillPromptBtn: HTMLButtonElement;

  // Toasts
  showSaveToastBtn: HTMLButtonElement;
  toastSkillIdInput: HTMLInputElement;
  toastSkillNameInput: HTMLInputElement;
  toastSkillIconInput: HTMLInputElement;
  showSaveAndInvokeToastBtn: HTMLButtonElement;
  deleteToastSkillIdInput: HTMLInputElement;
  showDeleteToastBtn: HTMLButtonElement;

  // Skill Invocation
  invokeSkillIdInput: HTMLInputElement;
  invokeSkillNameInput: HTMLInputElement;
  invokeSkillIconInput: HTMLInputElement;
  invokeSkillBtn: HTMLButtonElement;

  // Send Prompt
  promptInput: HTMLTextAreaElement;
  sendPromptBtn: HTMLButtonElement;

  // Full Page Editor
  editorUrlInput: HTMLInputElement;
  editorSkillNameInput: HTMLInputElement;
  editorSkillDescInput: HTMLInputElement;
  editorSkillInstructionsInput: HTMLTextAreaElement;
  editorSkillIconInput: HTMLInputElement;
  openEditorBtn: HTMLButtonElement;

  // Open URL
  openUrlInput: HTMLInputElement;
  openUrlBtn: HTMLButtonElement;

  // Close Dialog
  closeDialogBtn: HTMLButtonElement;

  // Performance Metrics
  metricFrameworkLoadTime: HTMLInputElement;
  sendFrameworkMetricBtn: HTMLButtonElement;
  metricWebClientLoadTime: HTMLInputElement;
  sendWebClientMetricBtn: HTMLButtonElement;
  metricDataFetchTime: HTMLInputElement;
  sendDataFetchMetricBtn: HTMLButtonElement;
  metricDataSaveTime: HTMLInputElement;
  sendDataSaveMetricBtn: HTMLButtonElement;
  sendAllMetricsBtn: HTMLButtonElement;

  // Navigation Simulation
  navBrowseBtn: HTMLButtonElement;
  navYourSkillsBtn: HTMLButtonElement;
  navAddDialogBtn: HTMLButtonElement;
  navEditDialogBtn: HTMLButtonElement;
  navEditorBtn: HTMLButtonElement;
  customNavPathInput: HTMLInputElement;
  customNavBtn: HTMLButtonElement;

  // Logs
  clearLogBtn: HTMLButtonElement;
  eventLog: HTMLDivElement;
  status: HTMLElement;
}

export const $: PageElementTypes = new Proxy({} as PageElementTypes, {
  get(_target: any, prop: string) {
    return document.getElementById(prop);
  },
});
