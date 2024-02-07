// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {ModuleMetricsProxy} from 'chrome://welcome/shared/module_metrics_proxy.js';

export class TestMetricsProxy extends TestBrowserProxy implements
    ModuleMetricsProxy {
  constructor() {
    super([
      'recordChoseAnOptionAndChoseNext',
      'recordChoseAnOptionAndChoseSkip',
      'recordChoseAnOptionAndNavigatedAway',
      'recordClickedDisabledNextButtonAndChoseNext',
      'recordClickedDisabledNextButtonAndChoseSkip',
      'recordClickedDisabledNextButtonAndNavigatedAway',
      'recordDidNothingAndChoseNext',
      'recordDidNothingAndChoseSkip',
      'recordDidNothingAndNavigatedAway',
      'recordNavigatedAwayThroughBrowserHistory',
      'recordPageShown',
    ]);
  }

  recordChoseAnOptionAndChoseNext() {
    this.methodCalled('recordChoseAnOptionAndChoseNext');
  }

  recordChoseAnOptionAndChoseSkip() {
    this.methodCalled('recordChoseAnOptionAndChoseSkip');
  }

  recordChoseAnOptionAndNavigatedAway() {
    this.methodCalled('recordChoseAnOptionAndNavigatedAway');
  }

  recordClickedDisabledNextButtonAndChoseNext() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseNext');
  }

  recordClickedDisabledNextButtonAndChoseSkip() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseSkip');
  }

  recordClickedDisabledNextButtonAndNavigatedAway() {
    this.methodCalled('recordClickedDisabledNextButtonAndNavigatedAway');
  }

  recordDidNothingAndChoseNext() {
    this.methodCalled('recordDidNothingAndChoseNext');
  }

  recordDidNothingAndChoseSkip() {
    this.methodCalled('recordDidNothingAndChoseSkip');
  }

  recordDidNothingAndNavigatedAway() {
    this.methodCalled('recordDidNothingAndNavigatedAway');
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }

  recordPageShown() {
    this.methodCalled('recordPageShown');
  }
}
