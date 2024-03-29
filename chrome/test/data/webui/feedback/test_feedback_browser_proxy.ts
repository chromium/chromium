// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FeedbackBrowserProxy} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFeedbackBrowserProxy extends TestBrowserProxy implements
    FeedbackBrowserProxy {
  private dialogArugments_: string = '';
  private systemLogs_: chrome.feedbackPrivate.LogsMapEntry[] = [];
  private userEmail_: string = '';
  private userMedia_: Promise<MediaStream|undefined> =
      Promise.resolve(undefined);

  constructor() {
    super([
      'closeDialog', 'getSystemInformation', 'getUserEmail',
      'getDialogArguments', 'getUserMedia', 'sendFeedback',
      'showAutofillMetadataInfo', 'showDialog', 'showMetrics', 'showSystemInfo',
    ]);
  }

  getSystemInformation(): Promise<chrome.feedbackPrivate.LogsMapEntry[]> {
    this.methodCalled('getSystemInformation');
    return Promise.resolve(this.systemLogs_);
  }

  setSystemInfomation(value: chrome.feedbackPrivate.LogsMapEntry[]) {
    this.systemLogs_ = value;
  }

  getUserEmail(): Promise<string> {
    this.methodCalled('getUserEmail');
    return Promise.resolve(this.userEmail_);
  }

  setUserEmail(value: string) {
    this.userEmail_ = value;
  }

  getDialogArguments(): string {
    this.methodCalled('getDialogArguments');
    return this.dialogArugments_;
  }

  setDialogArguments(value: string) {
    this.dialogArugments_ = value;
  }

  getUserMedia(_params: any): Promise<MediaStream|undefined> {
    this.methodCalled('getUserMedia');
    return this.userMedia_;
  }

  setUserMedia(value: Promise<MediaStream|undefined>) {
    this.userMedia_ = value;
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

  showDialog() {
    this.methodCalled('showDialog');
  }

  closeDialog() {
    this.methodCalled('closeDialog');
  }

  showSystemInfo() {
    this.methodCalled('showSystemInfo');
  }
  showMetrics() {
    this.methodCalled('showMetrics');
  }
  showAutofillMetadataInfo(autofillMetadata: string) {
    this.methodCalled('showAutofillMetadataInfo', autofillMetadata);
  }
}
