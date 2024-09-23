// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsRuntimeHostsDialogElement} from 'chrome://extensions/extensions.js';
import {getMatchingUserSpecifiedSites, getPatternFromSite} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {MetricsPrivateMock} from './test_util.js';

suite('RuntimeHostsDialog', function() {
  let dialog: ExtensionsRuntimeHostsDialogElement;
  let delegate: TestService;
  let metricsPrivateMock: MetricsPrivateMock;

  const ITEM_ID = 'a'.repeat(32);
  const userSiteSettings: chrome.developerPrivate.UserSiteSettings = {
    permittedSites: [],
    restrictedSites: [
      'http://restricted.com',
      'https://restricted.com:8080',
      'http://sub.restricted.com',
    ],
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('extensions-runtime-hosts-dialog');
    dialog.enableEnhancedSiteControls = true;

    delegate = new TestService();
    delegate.userSiteSettings = userSiteSettings;
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

  test('valid input', async function() {
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    const [id, pattern] = await delegate.whenCalled('addRuntimeHostPermission');
    assertEquals(ITEM_ID, id);
    assertEquals('http://www.example.com/*', pattern);
  });

  test('invalid input', async () => {
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
    await input.updateComplete;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertTrue(input.invalid);
    assertTrue(submit.disabled);

    // Entering valid text should clear the error and enable the submit button.
    input.value = 'http://www.example.com';
    await input.updateComplete;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);
    assertFalse(submit.disabled);
  });

  test('delegate indicates invalid input', async function() {
    delegate.acceptRuntimeHostPermission = false;

    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://....a';
    input.value = site;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    await delegate.whenCalled('addRuntimeHostPermission');
    assertTrue(input.invalid);
    assertTrue(submit.disabled);
  });

  test('editing current entry', async function() {
    const oldPattern = 'http://example.com/*';
    const newPattern = 'http://chromium.org/*';

    dialog.currentSite = oldPattern;
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = newPattern;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    const submit = dialog.$.submit;

    submit.click();
    let [id, pattern] =
        await delegate.whenCalled('removeRuntimeHostPermission');
    assertEquals(ITEM_ID, id);
    assertEquals(oldPattern, pattern);

    [id, pattern] = await delegate.whenCalled('addRuntimeHostPermission');
    assertEquals(ITEM_ID, id);
    assertEquals(newPattern, pattern);

    await eventToPromise('close', dialog);
    assertFalse(dialog.isOpen());
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.AddHostDialogSubmitted'),
        1);
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

  test('update site access', async function() {
    dialog.updateHostAccess = true;
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    const site = 'http://www.example.com';
    input.value = site;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();
    const [id, access] = await delegate.whenCalled('setItemHostAccess');
    assertEquals(ITEM_ID, id);
    assertEquals(chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES, access);
  });

  test('get matching user specified sites', function() {
    // Invalid pattern returns no matches.
    assertDeepEquals(
        [], getMatchingUserSpecifiedSites(['https://google.com'], 'invalid'));

    // Scheme match.
    assertDeepEquals(
        [],
        getMatchingUserSpecifiedSites(
            ['https://google.com'], 'http://google.com'));
    assertDeepEquals(
        ['https://google.com'],
        getMatchingUserSpecifiedSites(['https://google.com'], 'google.com'));

    // Subdomain and hostname match.
    assertDeepEquals(
        ['https://sub.restricted.com'],
        getMatchingUserSpecifiedSites(
            [
              'http://restricted.com',
              'https://sub.restricted.com',
              'other.com',
            ],
            '*://sub.restricted.com'));

    assertDeepEquals(
        ['http://restricted.com', 'https://sub.restricted.com'],
        getMatchingUserSpecifiedSites(
            [
              'http://restricted.com',
              'https://sub.restricted.com',
              'other.com',
            ],
            '*://*.restricted.com'));

    // Port match.
    assertDeepEquals(
        ['https://google.com:8080'],
        getMatchingUserSpecifiedSites(
            [
              'https://google.com:8080',
              'https://google.com:1337',
              'https://google.com',
            ],
            '*://google.com:8080'));

    assertDeepEquals(
        ['https://google.com:1337', 'https://google.com'],
        getMatchingUserSpecifiedSites(
            ['https://google.com:1337', 'https://google.com'],
            '*://google.com'));
  });

  test('adding site removes matching restricted sites', async function() {
    await delegate.whenCalled('getUserSiteSettings');
    flush();

    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = 'http://www.nomatch.com';
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);
    assertFalse(isVisible(dialog.shadowRoot!.querySelector(
        '.matching-restricted-sites-warning')));

    input.value = 'http://*.restricted.com';
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;
    assertFalse(input.invalid);
    assertTrue(isVisible(dialog.shadowRoot!.querySelector(
        '.matching-restricted-sites-warning')));

    const submit = dialog.$.submit;
    assertFalse(submit.disabled);
    submit.click();

    const [id, host] = await delegate.whenCalled('addRuntimeHostPermission');
    assertEquals(ITEM_ID, id);
    assertEquals('http://*.restricted.com/*', host);

    const [siteSet, removedSites] =
        await delegate.whenCalled('removeUserSpecifiedSites');
    assertEquals(chrome.developerPrivate.SiteSet.USER_RESTRICTED, siteSet);
    assertDeepEquals(
        ['http://restricted.com', 'http://sub.restricted.com'], removedSites);
  });
});
