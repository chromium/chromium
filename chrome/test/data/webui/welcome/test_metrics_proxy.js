// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {ModuleMetricsProxy} */
export class TestMetricsProxy extends TestBrowserProxy {
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

  /** @override */
  recordChoseAnOptionAndChoseNext() {
    this.methodCalled('recordChoseAnOptionAndChoseNext');
  }

  /** @override */
  recordChoseAnOptionAndChoseSkip() {
    this.methodCalled('recordChoseAnOptionAndChoseSkip');
  }

  /** @override */
  recordChoseAnOptionAndNavigatedAway() {
    this.methodCalled('recordChoseAnOptionAndNavigatedAway');
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseNext() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseNext');
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseSkip() {
    this.methodCalled('recordClickedDisabledNextButtonAndChoseSkip');
  }

  /** @override */
  recordClickedDisabledNextButtonAndNavigatedAway() {
    this.methodCalled('recordClickedDisabledNextButtonAndNavigatedAway');
  }

  /** @override */
  recordDidNothingAndChoseNext() {
    this.methodCalled('recordDidNothingAndChoseNext');
  }

  /** @override */
  recordDidNothingAndChoseSkip() {
    this.methodCalled('recordDidNothingAndChoseSkip');
  }

  /** @override */
  recordDidNothingAndNavigatedAway() {
    this.methodCalled('recordDidNothingAndNavigatedAway');
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }

  /** @override */
  recordPageShown() {
    this.methodCalled('recordPageShown');
  }
}
