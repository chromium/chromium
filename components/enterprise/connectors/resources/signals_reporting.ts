// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {BrowserProxy} from './browser_proxy.js';
import type {PageHandlerInterface, SignalsReportingState} from './connectors_internals.mojom-webui.js';
import {SignalsSource} from './connectors_internals.mojom-webui.js';
import {mojoDictionaryToJsonString} from './connectors_utils.js';
import {getTemplate} from './signals_reporting.html.js';

const SignalsSourceStringMap: Record<SignalsSource, string> = {
  [SignalsSource.kLastUploadedReport]: 'Last successful upload',
  [SignalsSource.kCollectedOnDemand]: 'Collected on demand',
};

export class SignalsReportingElement extends CustomElement {
  static get is() {
    return 'signals-reporting';
  }

  static override get template() {
    return getTemplate();
  }

  set errorText(errorText: string) {
    this.setValueToElement('#error-text', errorText);
  }

  set statusReportEnabledString(isEnabledString: string) {
    this.setValueToElement('#status-reporting-enabled', isEnabledString);
  }

  set signalsReportEnabledString(isEnabledString: string) {
    this.setValueToElement('#signals-reporting-enabled', isEnabledString);
  }

  set lastUploadAttemptTimestamp(timestamp: string) {
    this.setValueToElement('#last-upload-attempt-timestamp', timestamp);
  }

  set lastUploadSuccessTimestamp(timestamp: string) {
    this.setValueToElement('#last-upload-success-timestamp', timestamp);
  }

  set lastSignalsUploadConfigString(config: string) {
    this.setValueToElement('#last-signals-upload-config', config);
  }

  set canCollectAllFieldsString(canCollectAllFields: string) {
    this.setValueToElement('#can-collect-all', canCollectAllFields);
  }

  set signalsSourceString(source: string) {
    this.setValueToElement('#signals-source', source);
  }

  set signalsJsonString(jsonString: string) {
    this.setValueToElement('#signals-json', jsonString);
  }

  private get pageHandler(): PageHandlerInterface {
    return BrowserProxy.getInstance().handler;
  }

  constructor() {
    super();

    this.fetchSignalsReportingState();
  }

  private updateState(state: SignalsReportingState|undefined) {
    if (!state) {
      this.errorText = 'Can\'t retrieve signals reporting state.';
      this.showErrorElement();
      return;
    }

    if (state.errorInfo) {
      this.errorText = state.errorInfo;
      this.showErrorElement();
    } else {
      this.hideErrorElement();
    }

    this.statusReportEnabledString = `${state.statusReportEnabled}`;
    this.signalsReportEnabledString = `${state.signalsReportEnabled}`;
    this.lastUploadAttemptTimestamp = state.lastUploadAttemptTimestamp;
    this.lastUploadSuccessTimestamp = state.lastUploadSuccessTimestamp;
    this.lastSignalsUploadConfigString = state.lastSignalsUploadConfig;
    this.canCollectAllFieldsString = `${state.canCollectAllFields}`;
    if (state.signals) {
      this.signalsSourceString =
          SignalsSourceStringMap[state.signals.source] || 'None';
      this.signalsJsonString = mojoDictionaryToJsonString(state.signals.values);
    } else {
      this.signalsSourceString = 'None';
      this.signalsJsonString = '{}';
    }
  }

  private fetchSignalsReportingState() {
    this.pageHandler.getSignalsReportingState().then(
        (response: {state: SignalsReportingState}) =>
            this.updateState(response?.state),
        err => console.error(
            `fetchSignalsReportingState failed: ${JSON.stringify(err)}`));
  }

  private showErrorElement() {
    this.getRequiredElement('#error-section')?.classList.remove('hidden');
  }

  private hideErrorElement() {
    this.getRequiredElement('#error-section')?.classList.add('hidden');
  }

  private setValueToElement(elementId: string, stringValue: string) {
    const htmlElement = (this.$(elementId) as HTMLElement);
    if (htmlElement) {
      htmlElement.innerText = stringValue;
    } else {
      console.error(`Could not find ${elementId} element.`);
    }
  }
}

customElements.define(SignalsReportingElement.is, SignalsReportingElement);
