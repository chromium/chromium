// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CloseReason, ComposeState, InputMode, OpenMetadata} from 'chrome-untrusted://compose/compose.mojom-webui.js';
import {ComposeUntrustedDialogCallbackRouter, StyleModifier, UserFeedback} from 'chrome-untrusted://compose/compose.mojom-webui.js';
import type {ComposeApiProxy} from 'chrome-untrusted://compose/compose_api_proxy.js';
import {ComposeStatus} from 'chrome-untrusted://compose/compose_enums.mojom-webui.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

function getDefaultComposeState(): ComposeState {
  return {
    webuiState: '',
    feedback: UserFeedback.kUserFeedbackUnspecified,
    hasPendingRequest: false,
    response: null,
  };
}

function getDefaultOpenMetadata(): OpenMetadata {
  return {
    freComplete: true,
    msbbState: true,
    composeState: getDefaultComposeState(),
    initialInput: '',
    textSelected: false,
    configurableParams: {
      minWordLimit: 2,
      maxWordLimit: 50,
      maxCharacterLimit: 100,
    },
  };
}

function getDefaultUndoRedoComposeState(): ComposeState {
  return {
    hasPendingRequest: false,
    response: {
      status: ComposeStatus.kOk,
      undoAvailable: true,
      redoAvailable: true,
      providedByUser: false,
      result: 'some result',
      onDeviceEvaluationUsed: false,
      triggeredFromModifier: false,
    },
    webuiState: JSON.stringify({
      input: 'some other input',
      selectedLength: Number(StyleModifier.kUnset),
      selectedTone: Number(StyleModifier.kUnset),
    }),
    feedback: UserFeedback.kUserFeedbackUnspecified,
  };
}

export class TestComposeApiProxy extends TestBrowserProxy implements
    ComposeApiProxy {
  private openMetadata_: OpenMetadata = getDefaultOpenMetadata();
  private router_: ComposeUntrustedDialogCallbackRouter =
      new ComposeUntrustedDialogCallbackRouter();
  remote = this.router_.$.bindNewPipeAndPassRemote();
  private undoResponse_: ComposeState|null = null;
  private responseBeforeError_: ComposeState|null = null;
  private redoResponse_: ComposeState|null = null;

  constructor() {
    super([
      'acceptComposeResult',
      'logCancelEdit',
      'closeUi',
      'compose',
      'rewrite',
      'logEditInput',
      'openBugReportingLink',
      'openComposeLearnMorePage',
      'openFeedbackSurveyLink',
      'openSignInPage',
      'requestInitialState',
      'saveWebuiState',
      'setUserFeedback',
      'showUi',
      'undo',
      'recoverFromErrorState',
      'editResult',
      'redo',
    ]);
  }

  acceptComposeResult(): Promise<boolean> {
    this.methodCalled('acceptComposeResult');
    return Promise.resolve(true);
  }

  logCancelEdit() {
    this.methodCalled('logCancelEdit');
  }

  completeFirstRun() {}

  acknowledgeConsentDisclaimer() {}

  approveConsent() {}

  closeUi(reason: CloseReason) {
    this.methodCalled('closeUi', reason);
  }

  compose(input: string, mode: InputMode, edited: boolean): void {
    this.methodCalled('compose', {input, mode, edited});
  }

  rewrite(style: StyleModifier): void {
    this.methodCalled('rewrite', style);
  }

  logEditInput() {
    this.methodCalled('logEditInput');
  }

  undo(): Promise<(ComposeState | null)> {
    this.methodCalled('undo');
    return Promise.resolve(this.undoResponse_);
  }

  recoverFromErrorState(): Promise<(ComposeState | null)> {
    this.methodCalled('recoverFromErrorState');
    return Promise.resolve(this.responseBeforeError_);
  }

  editResult(result: string): Promise<boolean> {
    this.methodCalled('editResult', result);
    return Promise.resolve(false);
  }

  redo(): Promise<(ComposeState | null)> {
    this.methodCalled('redo');
    return Promise.resolve(this.redoResponse_);
  }

  getRouter() {
    return this.router_;
  }

  openBugReportingLink() {
    this.methodCalled('openBugReportingLink');
  }

  openComposeLearnMorePage() {
    this.methodCalled('openComposeLearnMorePage');
  }

  openFeedbackSurveyLink() {
    this.methodCalled('openFeedbackSurveyLink');
  }

  openSignInPage() {
    this.methodCalled('openSignInPage');
  }

  openComposeSettings() {}

  requestInitialState(): Promise<OpenMetadata> {
    this.methodCalled('requestInitialState');
    return Promise.resolve(this.openMetadata_);
  }

  saveWebuiState(state: string) {
    this.methodCalled('saveWebuiState', state);
  }

  setUserFeedback(feedback: UserFeedback) {
    this.methodCalled('setUserFeedback', feedback);
  }

  setOpenMetadata(
      overrides: Partial<OpenMetadata>,
      composeStateOverrides: Partial<ComposeState> = {}) {
    this.openMetadata_ = Object.assign(getDefaultOpenMetadata(), overrides, {
      composeState:
          Object.assign(getDefaultComposeState(), composeStateOverrides),
    });
  }

  setUndoResponse(state: ComposeState|null) {
    this.undoResponse_ = state;
  }

  setUndoResponseWithUndoAndRedo(
      undoAvailable: boolean, redoAvailable: boolean) {
    this.undoResponse_ = Object.assign(getDefaultUndoRedoComposeState());
    this !.undoResponse_!.response!.undoAvailable = undoAvailable;
    this !.undoResponse_!.response!.redoAvailable = redoAvailable;
  }

  setResponseBeforeError(state: ComposeState|null) {
    this.responseBeforeError_ = state;
  }

  setRedoResponse(state: ComposeState|null) {
    this.redoResponse_ = state;
  }

  setRedoResponseWithUndoAndRedo(
      undoAvailable: boolean, redoAvailable: boolean) {
    this.redoResponse_ = Object.assign(getDefaultUndoRedoComposeState());
    this !.redoResponse_!.response!.undoAvailable = undoAvailable;
    this !.redoResponse_!.response!.redoAvailable = redoAvailable;
  }

  showUi() {
    this.methodCalled('showUi');
  }
}
