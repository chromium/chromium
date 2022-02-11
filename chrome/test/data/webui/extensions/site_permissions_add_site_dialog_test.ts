// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-permissions-add-site-dialog. */
import 'chrome://extensions/extensions.js';

import {getSitePermissionsPatternFromSite, SitePermissionsAddSiteDialogElement} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestService} from './test_service.js';

suite('SitePermissionsAddSiteDialog', function() {
  let element: SitePermissionsAddSiteDialogElement;
  let delegate: TestService;

  setup(function() {
    delegate = new TestService();

    document.body.innerHTML = '';
    element = document.createElement('site-permissions-add-site-dialog');
    element.delegate = delegate;
    element.siteSet = chrome.developerPrivate.UserSiteSet.PERMITTED;
    document.body.appendChild(element);
  });

  test('valid input', async function() {
    const input = element.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    input.fire('input');
    assertFalse(input.invalid);

    const submit = element.$.submit;
    assertFalse(submit.disabled);
    submit.click();

    const [siteSet, host] = await delegate.whenCalled('addUserSpecifiedSite');
    assertEquals(chrome.developerPrivate.UserSiteSet.PERMITTED, siteSet);
    assertEquals('http://www.example.com', host);
  });

  test('invalid input', function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = element.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    assertFalse(input.invalid);
    const submit = element.$.submit;
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

  test('get pattern from url', function() {
    assertEquals(
        'http://example.com',
        getSitePermissionsPatternFromSite('http://example.com'));
    assertEquals(
        'https://example.com',
        getSitePermissionsPatternFromSite('example.com'));
    assertEquals(
        'https://example.com:80',
        getSitePermissionsPatternFromSite('https://example.com:80'));
    assertEquals(
        'https://subdomain.site.org:3030',
        getSitePermissionsPatternFromSite('subdomain.site.org:3030'));
  });
});
