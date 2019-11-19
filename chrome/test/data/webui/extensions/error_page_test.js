// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */
import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isVisible} from '../test_util.m.js';

import {ClickMock, createExtensionInfo} from './test_util.js';

window.extension_error_page_tests = {};
extension_error_page_tests.suiteName = 'ExtensionErrorPageTest';
/** @enum {string} */
extension_error_page_tests.TestNames = {
  Layout: 'layout',
  CodeSection: 'code section',
  ErrorSelection: 'error selection',
};

/** @implements {ErrorPageDelegate} */
class MockErrorPageDelegate extends ClickMock {
  /** @override */
  deleteErrors(extensionId, errorIds, type) {}

  /** @override */
  requestFileSource(args) {
    this.requestFileSourceArgs = args;
    this.requestFileSourceResolver = new PromiseResolver();
    return this.requestFileSourceResolver.promise;
  }
}

suite(extension_error_page_tests.suiteName, function() {
  /** @type {chrome.developerPrivate.ExtensionInfo} */
  let extensionData;

  /** @type {ExtensionsErrorPageElement} */
  let errorPage;

  /** @type {MockErrorPageDelegate} */
  let mockDelegate;

  const extensionId = 'a'.repeat(32);

  // Common data for runtime errors.
  const runtimeErrorBase = {
    type: chrome.developerPrivate.ErrorType.RUNTIME,
    extensionId: extensionId,
    fromIncognito: false,
  };

  // Common data for manifest errors.
  const manifestErrorBase = {
    type: chrome.developerPrivate.ErrorType.MANIFEST,
    extensionId: extensionId,
    fromIncognito: false,
  };

  // Initialize an extension item before each test.
  setup(function() {
    PolymerTest.clearBody();
    const runtimeError = Object.assign(
        {
          source: 'chrome-extension://' + extensionId + '/source.html',
          message: 'message',
          id: 1,
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
        },
        runtimeErrorBase);
    extensionData = createExtensionInfo({
      runtimeErrors: [runtimeError],
      manifestErrors: [],
    });
    errorPage = document.createElement('extensions-error-page');
    mockDelegate = new MockErrorPageDelegate();
    errorPage.delegate = mockDelegate;
    errorPage.data = extensionData;
    document.body.appendChild(errorPage);
  });

  test(assert(extension_error_page_tests.TestNames.Layout), function() {
    flush();

    const testIsVisible = isVisible.bind(null, errorPage);
    expectTrue(testIsVisible('#closeButton'));
    expectTrue(testIsVisible('#heading'));
    expectTrue(testIsVisible('#errorsList'));

    let errorElements = errorPage.shadowRoot.querySelectorAll('.error-item');
    expectEquals(1, errorElements.length);
    let error = errorElements[0];
    expectEquals(
        'message', error.querySelector('.error-message').textContent.trim());
    expectTrue(error.querySelector('iron-icon').icon == 'cr:error');

    const manifestError = Object.assign(
        {
          source: 'manifest.json',
          message: 'invalid key',
          id: 2,
          manifestKey: 'permissions',
        },
        manifestErrorBase);
    errorPage.set('data.manifestErrors', [manifestError]);
    flush();
    errorElements = errorPage.shadowRoot.querySelectorAll('.error-item');
    expectEquals(2, errorElements.length);
    error = errorElements[0];
    expectEquals(
        'invalid key',
        error.querySelector('.error-message').textContent.trim());
    expectTrue(error.querySelector('iron-icon').icon == 'cr:warning');

    mockDelegate.testClickingCalls(
        error.querySelector('.icon-delete-gray'), 'deleteErrors',
        [extensionId, [manifestError.id]]);
  });

  test(
      assert(extension_error_page_tests.TestNames.CodeSection), function(done) {
        flush();

        assertTrue(!!mockDelegate.requestFileSourceArgs);
        const args = mockDelegate.requestFileSourceArgs;
        expectEquals(extensionId, args.extensionId);
        expectEquals('source.html', args.pathSuffix);
        expectEquals('message', args.message);

        expectTrue(!!mockDelegate.requestFileSourceResolver);
        const code = {
          beforeHighlight: 'foo',
          highlight: 'bar',
          afterHighlight: 'baz',
          message: 'quu',
        };
        mockDelegate.requestFileSourceResolver.resolve(code);
        mockDelegate.requestFileSourceResolver.promise.then(function() {
          flush();
          expectEquals(code, errorPage.$$('extensions-code-section').code);
          done();
        });
      });

  test(assert(extension_error_page_tests.TestNames.ErrorSelection), function() {
    const nextRuntimeError = Object.assign(
        {
          source: 'chrome-extension://' + extensionId + '/other_source.html',
          message: 'Other error',
          id: 2,
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
          renderProcessId: 111,
          renderViewId: 222,
          canInspect: true,
          contextUrl: 'http://test.com',
          stackTrace: [{url: 'url', lineNumber: 123, columnNumber: 321}],
        },
        runtimeErrorBase);
    // Add a new runtime error to the end.
    errorPage.push('data.runtimeErrors', nextRuntimeError);
    flush();

    const errorElements =
        errorPage.shadowRoot.querySelectorAll('.error-item .start');
    const ironCollapses =
        errorPage.shadowRoot.querySelectorAll('iron-collapse');
    expectEquals(2, errorElements.length);
    expectEquals(2, ironCollapses.length);

    // The first error should be focused by default, and we should have
    // requested the source for it.
    expectEquals(extensionData.runtimeErrors[0], errorPage.getSelectedError());
    expectTrue(!!mockDelegate.requestFileSourceArgs);
    let args = mockDelegate.requestFileSourceArgs;
    expectEquals('source.html', args.pathSuffix);
    expectTrue(ironCollapses[0].opened);
    expectFalse(ironCollapses[1].opened);
    mockDelegate.requestFileSourceResolver.resolve(null);

    mockDelegate.requestFileSourceResolver = new PromiseResolver();
    mockDelegate.requestFileSourceArgs = undefined;

    // Tap the second error. It should now be selected and we should request
    // the source for it.
    errorElements[1].click();
    expectEquals(nextRuntimeError, errorPage.getSelectedError());
    expectTrue(!!mockDelegate.requestFileSourceArgs);
    args = mockDelegate.requestFileSourceArgs;
    expectEquals('other_source.html', args.pathSuffix);
    expectTrue(ironCollapses[1].opened);
    expectFalse(ironCollapses[0].opened);

    expectEquals(
        'Unknown',
        ironCollapses[0].querySelector('.context-url').textContent.trim());
    expectEquals(
        nextRuntimeError.contextUrl,
        ironCollapses[1].querySelector('.context-url').textContent.trim());
  });
});
