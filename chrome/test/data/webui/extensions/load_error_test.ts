// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-load-error. */

import 'chrome://extensions/extensions.js';

import type {LoadErrorElement} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {isElementVisible} from './test_util.js';

suite('ExtensionLoadErrorTests', function() {
  let loadError: LoadErrorElement;

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

  test('RetryError', async function() {
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

  test('RetrySuccess', async function() {
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

  test('CodeSection', async () => {
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
    await microtasksFinished();
    assertFalse(loadError.$.code.shadowRoot!
                    .querySelector<HTMLElement>('#scroll-container')!.hidden);
  });

  test('PathWithoutSource', async () => {
    loadError.loadError = stubLoadError;
    await microtasksFinished();

    // File should be visible with name.
    const fileRow = loadError.shadowRoot!.querySelector<HTMLElement>('#file')!;
    assertFalse(fileRow.hidden);
    assertEquals(
        fileRow.querySelector<HTMLSpanElement>('.row-value')!.innerText,
        'some/path/');
  });

  test('GenericError', async () => {
    assertTrue(loadError.$.code.shadowRoot!
                   .querySelector<HTMLElement>('#scroll-container')!.hidden);

    loadError.loadError = new Error('Some generic error');
    await microtasksFinished();

    // Code section should still be hidden because there is no source.
    assertTrue(loadError.$.code.shadowRoot!
                   .querySelector<HTMLElement>('#scroll-container')!.hidden);

    // File row should be hidden because there is no specific file.
    assertTrue(
        loadError.shadowRoot!.querySelector<HTMLElement>('#file')!.hidden);

    // Error should be visible with message.
    const errorRow =
        loadError.shadowRoot!.querySelector<HTMLElement>('#error')!;
    assertFalse(errorRow.hidden);
    assertEquals(
        errorRow.querySelector<HTMLSpanElement>('.row-value')!.innerText,
        'Some generic error');
  });
});
