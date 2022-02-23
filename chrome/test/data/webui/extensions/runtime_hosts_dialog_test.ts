// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {ExtensionsRuntimeHostsDialogElement, getPatternFromSite} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {MetricsPrivateMock} from './test_util.js';

suite('RuntimeHostsDialog', function() {
  let dialog: ExtensionsRuntimeHostsDialogElement;
  let delegate: TestService;
  let metricsPrivateMock: MetricsPrivateMock;

  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    document.body.innerHTML = '';
    dialog = document.createElement('extensions-runtime-hosts-dialog');

    delegate = new TestService();
    dialog.delegate = delegate;
    dialog.itemId = ITEM_ID;

    document.body.appendChild(dialog);

    metricsPrivateMock = new MetricsPrivateMock();
    chrome.metricsPrivate =
        metricsPrivateMock as unknown as typeof chrome.metricsPrivate;
  });

  teardown(function() {
    dialog.remove();
  });

  test('valid input', function() {
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    return delegate.whenCalled('addRuntimeHostPermission').then((args) => {
      let id = args[0];
      let input = args[1];
      assertEquals(ITEM_ID, id);
      assertEquals('http://www.example.com/*', input);
    });
  });

  test('invalid input', function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    assertFalse(input.invalid);
    const submit = dialog.$.submit;
    assertTrue(submit.disabled);

    // Simulate user input of invalid text.
    const invalidSite = 'foobar';
    input.value = invalidSite;
    input.fire('input');
    assertTrue(input.invalid);
    assertTrue(submit.disabled);

    // Entering valid text should clear the error and enable the submit button.
    input.value = 'http://www.example.com';
    input.fire('input');
    assertFalse(input.invalid);
    assertFalse(submit.disabled);
  });

  test('delegate indicates invalid input', function() {
    delegate.acceptRuntimeHostPermission = false;

    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://....a';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    return delegate.whenCalled('addRuntimeHostPermission').then(() => {
      assertTrue(input.invalid);
      assertTrue(submit.disabled);
    });
  });

  test('editing current entry', function() {
    const oldPattern = 'http://example.com/*';
    const newPattern = 'http://chromium.org/*';

    dialog.currentSite = oldPattern;
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = newPattern;
    input.fire('input');
    const submit = dialog.$.submit;

    submit.click();
    return delegate.whenCalled('removeRuntimeHostPermission')
        .then((args) => {
          assertEquals(ITEM_ID, args[0] /* id */);
          assertEquals(oldPattern, args[1] /* pattern */);
          return delegate.whenCalled('addRuntimeHostPermission');
        })
        .then((args) => {
          assertEquals(ITEM_ID, args[0] /* id */);
          assertEquals(newPattern, args[1] /* pattern */);
          return eventToPromise('close', dialog);
        })
        .then(() => {
          assertFalse(dialog.isOpen());
          assertEquals(
              metricsPrivateMock.getUserActionCount(
                  'Extensions.Settings.Hosts.AddHostDialogSubmitted'),
              1);
        });
  });

  test('get pattern from url', function() {
    assertEquals(
        'https://example.com/*', getPatternFromSite('https://example.com/*'));
    assertEquals(
        'https://example.com/*', getPatternFromSite('https://example.com/'));
    assertEquals(
        'https://example.com/*', getPatternFromSite('https://example.com'));
    assertEquals(
        'https://*.example.com/*',
        getPatternFromSite('https://*.example.com/*'));
    assertEquals('*://example.com/*', getPatternFromSite('example.com'));
    assertEquals(
        'https://example.com:80/*',
        getPatternFromSite('https://example.com:80/*'));
    assertEquals(
        'http://localhost:3030/*', getPatternFromSite('http://localhost:3030'));
  });

  test('update site access', function() {
    dialog.updateHostAccess = true;
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    return delegate.whenCalled('setItemHostAccess').then((args) => {
      let id = args[0];
      let access = args[1];
      assertEquals(ITEM_ID, id);
      assertEquals(
          chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES, access);
    });
  });
});
