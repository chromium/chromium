// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-permissions-edit-url-dialog. */
import 'chrome://extensions/extensions.js';

import type {SitePermissionsEditUrlDialogElement} from 'chrome://extensions/extensions.js';
import {getSitePermissionsPatternFromSite} from 'chrome://extensions/extensions.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';

suite('SitePermissionsEditUrlDialog', function() {
  let element: SitePermissionsEditUrlDialogElement;
  let delegate: TestService;

  setup(function() {
    delegate = new TestService();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('site-permissions-edit-url-dialog');
    element.delegate = delegate;
    element.siteSet = chrome.developerPrivate.SiteSet.USER_PERMITTED;
    document.body.appendChild(element);
  });

  test('valid input', async function() {
    const input = element.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    await microtasksFinished();
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertFalse(input.invalid);

    const submit = element.$.submit;
    assertFalse(submit.disabled);
    submit.click();

    const [siteSet, hosts] = await delegate.whenCalled('addUserSpecifiedSites');
    assertEquals(chrome.developerPrivate.SiteSet.USER_PERMITTED, siteSet);
    assertDeepEquals(['http://www.example.com'], hosts);
  });

  test('invalid input', async () => {
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
    await microtasksFinished();
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertTrue(input.invalid);
    assertTrue(submit.disabled);

    // Entering valid text should clear the error and enable the submit button.
    input.value = 'http://www.example.com';
    await microtasksFinished();
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertFalse(input.invalid);
    assertFalse(submit.disabled);

    // Wildcard scheme is considered invalid input.
    input.value = '*://www.example.com';
    await microtasksFinished();
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertTrue(input.invalid);
    assertTrue(submit.disabled);
  });

  test('editing current site', async function() {
    const oldSite = 'http://www.example.com';
    const newSite = 'https://www.google.com';
    element.siteToEdit = oldSite;

    const input = element.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = newSite;
    await microtasksFinished();
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await microtasksFinished();
    assertFalse(input.invalid);

    const whenClosed = eventToPromise('close', element);
    const submit = element.$.submit;
    assertFalse(submit.disabled);
    submit.click();

    const [removedSiteSet, removedSites] =
        await delegate.whenCalled('removeUserSpecifiedSites');
    assertEquals(
        chrome.developerPrivate.SiteSet.USER_PERMITTED, removedSiteSet);
    assertDeepEquals([oldSite], removedSites);

    const [addedSiteSet, addedSites] =
        await delegate.whenCalled('addUserSpecifiedSites');
    assertEquals(chrome.developerPrivate.SiteSet.USER_PERMITTED, addedSiteSet);
    assertDeepEquals([newSite], addedSites);

    await whenClosed;
    assertFalse(element.$.dialog.open);
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
