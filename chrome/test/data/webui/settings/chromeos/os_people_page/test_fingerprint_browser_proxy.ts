// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FingerprintBrowserProxy, FingerprintInfo, FingerprintResultType} from 'chrome://os-settings/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFingerprintBrowserProxy extends TestBrowserProxy implements
    FingerprintBrowserProxy {
  private fingerprintsList_: string[];
  constructor() {
    super([
      'getFingerprintsList',
      'getNumFingerprints',
      'startEnroll',
      'cancelCurrentEnroll',
      'getEnrollmentLabel',
      'removeEnrollment',
      'changeEnrollmentLabel',
      'fakeScanComplete',
    ]);
    this.fingerprintsList_ = [];
  }

  setFingerprints(fingerprints: string[]): void {
    this.fingerprintsList_ = fingerprints.slice();
  }

  scanReceived(
      result: FingerprintResultType, complete: boolean, percent: number): void {
    if (complete) {
      this.fingerprintsList_.push('New Label');
    }

    webUIListenerCallback(
        'on-fingerprint-scan-received',
        {result: result, isComplete: complete, percentComplete: percent});
  }

  getFingerprintsList(): Promise<FingerprintInfo> {
    this.methodCalled('getFingerprintsList');
    const fingerprintInfo: FingerprintInfo = {
      fingerprintsList: this.fingerprintsList_.slice(),
      isMaxed: this.fingerprintsList_.length >= 3,
    };
    return Promise.resolve(fingerprintInfo);
  }

  getNumFingerprints(): Promise<number> {
    this.methodCalled('getNumFingerprints');
    return Promise.resolve(this.fingerprintsList_.length);
  }

  startEnroll(): void {
    this.methodCalled('startEnroll');
  }

  cancelCurrentEnroll(): void {
    this.methodCalled('cancelCurrentEnroll');
  }

  getEnrollmentLabel(index: number): Promise<string> {
    this.methodCalled('getEnrollmentLabel');
    return Promise.resolve(this.fingerprintsList_[index] as string);
  }

  removeEnrollment(index: number): Promise<boolean> {
    this.fingerprintsList_.splice(index, 1);
    this.methodCalled('removeEnrollment', index);
    return Promise.resolve(true);
  }

  changeEnrollmentLabel(index: number, newLabel: string): Promise<boolean> {
    this.fingerprintsList_[index] = newLabel;
    this.methodCalled('changeEnrollmentLabel', index, newLabel);
    return Promise.resolve(true);
  }

  fakeScanComplete(): void {
    chrome.send('fakeScanComplete');
  }
}
