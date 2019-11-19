// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-load-error. */

import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {TestService} from './test_service.js';
import {isElementVisible} from './test_util.js';

window.extension_load_error_tests = {};
extension_load_error_tests.suiteName = 'ExtensionLoadErrorTests';
/** @enum {string} */
extension_load_error_tests.TestNames = {
  RetryError: 'RetryError',
  RetrySuccess: 'RetrySuccess',
  CodeSection: 'Code Section',
};

suite(extension_load_error_tests.suiteName, function() {
  /** @type {ExtensionsLoadErrorElement} */
  let loadError;

  /** @type {MockDelegate} */
  let mockDelegate;

  const fakeGuid = 'uniqueId';

  const stubLoadError = {
    error: 'error',
    path: 'some/path/',
    retryGuid: fakeGuid,
  };

  setup(function() {
    PolymerTest.clearBody();
    mockDelegate = new TestService();
    loadError = document.createElement('extensions-load-error');
    loadError.delegate = mockDelegate;
    loadError.loadError = stubLoadError;
    document.body.appendChild(loadError);
  });

  test(assert(extension_load_error_tests.TestNames.RetryError), function() {
    const dialogElement = loadError.$$('cr-dialog').getNative();
    expectFalse(isElementVisible(dialogElement));
    loadError.show();
    expectTrue(isElementVisible(dialogElement));

    mockDelegate.setRetryLoadUnpackedError(stubLoadError);
    loadError.$$('.action-button').click();
    return mockDelegate.whenCalled('retryLoadUnpacked').then(arg => {
      expectEquals(fakeGuid, arg);
      expectTrue(isElementVisible(dialogElement));
      loadError.$$('.cancel-button').click();
      expectFalse(isElementVisible(dialogElement));
    });
  });

  test(assert(extension_load_error_tests.TestNames.RetrySuccess), function() {
    const dialogElement = loadError.$$('cr-dialog').getNative();
    expectFalse(isElementVisible(dialogElement));
    loadError.show();
    expectTrue(isElementVisible(dialogElement));

    loadError.$$('.action-button').click();
    return mockDelegate.whenCalled('retryLoadUnpacked').then(arg => {
      expectEquals(fakeGuid, arg);
      expectFalse(isElementVisible(dialogElement));
    });
  });

  test(assert(extension_load_error_tests.TestNames.CodeSection), function() {
    expectTrue(loadError.$.code.$$('#scroll-container').hidden);
    const loadErrorWithSource = {
      error: 'Some error',
      path: '/some/path',
      source: {
        beforeHighlight: 'before',
        highlight: 'highlight',
        afterHighlight: 'after',
      },
    };

    loadError.loadError = loadErrorWithSource;
    expectFalse(loadError.$.code.$$('#scroll-container').hidden);
  });
});
