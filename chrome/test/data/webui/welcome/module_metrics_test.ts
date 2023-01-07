// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleMetricsManager} from 'chrome://welcome/shared/module_metrics_proxy.js';

import {TestMetricsProxy} from './test_metrics_proxy.js';

suite('ModuleMetricsTest', function() {
  let testMetricsProxy: TestMetricsProxy;
  let testMetricsManager: ModuleMetricsManager;

  setup(function() {
    testMetricsProxy = new TestMetricsProxy();
    testMetricsManager = new ModuleMetricsManager(testMetricsProxy);

    testMetricsManager.recordPageInitialized();

    return testMetricsProxy.whenCalled('recordPageShown');
  });

  test('do nothing, click skip', function() {
    testMetricsManager.recordNoThanks();
    return testMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
  });

  test('do nothing, click next', function() {
    testMetricsManager.recordGetStarted();
    return testMetricsProxy.whenCalled('recordDidNothingAndChoseNext');
  });

  test('do nothing, navigate away', function() {
    testMetricsManager.recordNavigatedAway();
    return testMetricsProxy.whenCalled('recordDidNothingAndNavigatedAway');
  });

  test('choose option, click skip', function() {
    testMetricsManager.recordClickedOption();
    testMetricsManager.recordNoThanks();
    return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseSkip');
  });

  test('choose option, click next', function() {
    testMetricsManager.recordClickedOption();
    testMetricsManager.recordGetStarted();
    return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseNext');
  });

  test('choose option, navigate away', function() {
    testMetricsManager.recordClickedOption();
    testMetricsManager.recordNavigatedAway();
    return testMetricsProxy.whenCalled('recordChoseAnOptionAndNavigatedAway');
  });

  test('click disabled next, click skip', function() {
    testMetricsManager.recordClickedDisabledButton();
    testMetricsManager.recordNoThanks();
    return testMetricsProxy.whenCalled(
        'recordClickedDisabledNextButtonAndChoseSkip');
  });

  test('click disabled next, click next', function() {
    testMetricsManager.recordClickedDisabledButton();
    // 'Next' should become enabled only after clicking another option.
    testMetricsManager.recordClickedOption();
    testMetricsManager.recordGetStarted();
    return testMetricsProxy.whenCalled(
        'recordClickedDisabledNextButtonAndChoseNext');
  });

  test('click disabled next, navigate away', function() {
    testMetricsManager.recordClickedDisabledButton();
    testMetricsManager.recordNavigatedAway();
    return testMetricsProxy.whenCalled(
        'recordClickedDisabledNextButtonAndNavigatedAway');
  });

  test('choose option, click disabled next, click next', function() {
    testMetricsManager.recordClickedOption();
    testMetricsManager.recordClickedDisabledButton();
    testMetricsManager.recordGetStarted();
    return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseNext');
  });
});
