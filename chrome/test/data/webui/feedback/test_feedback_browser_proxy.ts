// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedbackBrowserProxy} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFeedbackBrowserProxy extends TestBrowserProxy implements
    FeedbackBrowserProxy {
  private dialogArugments_: string = '';

  constructor() {
    super([
      'closeDialog',
      'sendFeedback',
    ]);
  }

  getSystemInformation(): Promise<chrome.feedbackPrivate.LogsMapEntry[]> {
    return Promise.resolve([]);
  }

  getUserEmail(): Promise<string> {
    return Promise.resolve('dummy_user_email');
  }

  getDialogArguments(): string {
    return this.dialogArugments_;
  }

  setDialogArguments(value: string) {
    this.dialogArugments_ = value;
  }

  getUserMedia(_params: any): Promise<MediaStream|undefined> {
    return Promise.resolve(undefined);
  }

  sendFeedback(
      feedback: chrome.feedbackPrivate.FeedbackInfo, _loadSystemInfo?: boolean,
      _formOpenTime?: number):
      Promise<chrome.feedbackPrivate.SendFeedbackResult> {
    this.methodCalled('sendFeedback', feedback);
    return Promise.resolve({
      status: chrome.feedbackPrivate.Status.SUCCESS,
      landingPageType: chrome.feedbackPrivate.LandingPageType.NORMAL,
    });
  }

  showDialog() {}

  closeDialog() {
    this.methodCalled('closeDialog');
  }

  // <if expr="chromeos_ash">
  showAssistantLogsInfo() {}
  showBluetoothLogsInfo() {}
  // </if>

  showSystemInfo() {}
  showMetrics() {}
  showAutofillMetadataInfo(_autofillMetadata: string) {}
}
