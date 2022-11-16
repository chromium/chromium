// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-load-error. */

import 'chrome://extensions/extensions.js';

import {ExtensionsLoadErrorElement} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestService} from './test_service.js';
import {isElementVisible} from './test_util.js';

const extension_load_error_tests = {
  suiteName: 'ExtensionLoadErrorTests',
  TestNames: {
    RetryError: 'RetryError',
    RetrySuccess: 'RetrySuccess',
    CodeSection: 'Code Section',
  },
};
Object.assign(window, {extension_load_error_tests});

suite(extension_load_error_tests.suiteName, function() {
  let loadError: ExtensionsLoadErrorElement;

  let mockDelegate: TestService;

  const fakeGuid: string = 'uniqueId';

  const stubLoadError = {
    error: 'error',
    path: 'some/path/',
    retryGuid: fakeGuid,
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockDelegate = new TestService();
    loadError = document.createElement('extensions-load-error');
    loadError.delegate = mockDelegate;
    loadError.loadError = stubLoadError;
    document.body.appendChild(loadError);
  });

  test(extension_load_error_tests.TestNames.RetryError, async function() {
    const dialogElement =
        loadError.shadowRoot!.querySelector('cr-dialog')!.getNative();
    assertFalse(isElementVisible(dialogElement));
    loadError.show();
    assertTrue(isElementVisible(dialogElement));

    mockDelegate.setRetryLoadUnpackedError(stubLoadError);
    loadError.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    const arg = await mockDelegate.whenCalled('retryLoadUnpacked');
    assertEquals(fakeGuid, arg);
    assertTrue(isElementVisible(dialogElement));
    loadError.shadowRoot!.querySelector<HTMLElement>('.cancel-button')!.click();
    assertFalse(isElementVisible(dialogElement));
  });

  test(extension_load_error_tests.TestNames.RetrySuccess, async function() {
    const dialogElement =
        loadError.shadowRoot!.querySelector('cr-dialog')!.getNative();
    assertFalse(isElementVisible(dialogElement));
    loadError.show();
    assertTrue(isElementVisible(dialogElement));

    loadError.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    const arg = await mockDelegate.whenCalled('retryLoadUnpacked');
    assertEquals(fakeGuid, arg);
    assertFalse(isElementVisible(dialogElement));
  });

  test(extension_load_error_tests.TestNames.CodeSection, function() {
    assertTrue(loadError.$.code.shadowRoot!
                   .querySelector<HTMLElement>('#scroll-container')!.hidden);
    const loadErrorWithSource = {
      error: 'Some error',
      path: '/some/path',
      retryGuid: '',
      source: {
        beforeHighlight: 'before',
        highlight: 'highlight',
        afterHighlight: 'after',
      },
    };

    loadError.loadError = loadErrorWithSource;
    assertFalse(loadError.$.code.shadowRoot!
                    .querySelector<HTMLElement>('#scroll-container')!.hidden);
  });
});
