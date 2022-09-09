// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for site-permissions-edit-permissions-dialog.
 * */
import 'chrome://extensions/extensions.js';

import {SitePermissionsEditPermissionsDialogElement} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';

suite('SitePermissionsEditPermissionsDialog', function() {
  let element: SitePermissionsEditPermissionsDialogElement;
  let delegate: TestService;
  const SiteSet = chrome.developerPrivate.SiteSet;

  setup(function() {
    delegate = new TestService();

    document.body.innerHTML = '';
    element =
        document.createElement('site-permissions-edit-permissions-dialog');
    element.delegate = delegate;
    element.site = 'http://example.com';
    element.originalSiteSet = SiteSet.USER_PERMITTED;
    document.body.appendChild(element);
  });

  test('editing current site set', async function() {
    const siteSetRadioGroup =
        element.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!siteSetRadioGroup);
    assertEquals(SiteSet.USER_PERMITTED, siteSetRadioGroup.selected);

    const restrictSiteRadioButton =
        element.shadowRoot!.querySelector<HTMLElement>(
            `cr-radio-button[name=${SiteSet.USER_RESTRICTED}]`);
    assertTrue(!!restrictSiteRadioButton);
    restrictSiteRadioButton.click();

    flush();
    assertEquals(SiteSet.USER_RESTRICTED, siteSetRadioGroup.selected);

    const whenClosed = eventToPromise('close', element);
    const submit = element.$.submit;
    submit.click();

    const [siteSet, sites] = await delegate.whenCalled('addUserSpecifiedSites');
    assertEquals(SiteSet.USER_RESTRICTED, siteSet);
    assertDeepEquals([element.site], sites);

    await whenClosed;
    assertFalse(element.$.dialog.open);
  });
});
