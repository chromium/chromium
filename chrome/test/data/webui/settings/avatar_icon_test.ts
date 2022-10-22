// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the AvatarIcon component. */

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

suite('AvatarIcon', function() {
  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
  });

  test('avatarOfFirstSignedInAccountIsDisplayed', async function() {
    syncBrowserProxy.storedAccounts = [
      {
        fullName: 'john doe',
        email: 'john@gmail.com',
        avatarImage: 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
            'LAAAAAABAAEAAAICTAEAOw==',
      },
      {
        fullName: 'john doe',
        email: 'john-other-account@gmail.com',
        avatarImage: 'data:image/gif;base64,R0lGODdhAgADAKEDAAAA//8AAAD/AP///' +
            'ywAAAAAAgADAAACBEwkAAUAOw==',
      },
    ];

    const avatarIcon = document.createElement('settings-avatar-icon');
    document.body.appendChild(avatarIcon);
    flush();

    await syncBrowserProxy.whenCalled('getStoredAccounts');

    assertEquals(
        syncBrowserProxy.storedAccounts[0]!.avatarImage,
        avatarIcon.shadowRoot!.querySelector('img')!.src);
  });

  test('fallbackAvatarDisplayedWhenSignedInUserHasNoAvatar', async function() {
    syncBrowserProxy.storedAccounts = [{
      fullName: 'john doe',
      email: 'john@gmail.com',
    }];

    const avatarIcon = document.createElement('settings-avatar-icon');
    document.body.appendChild(avatarIcon);
    flush();

    await syncBrowserProxy.whenCalled('getStoredAccounts');

    assertEquals(
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE',
        avatarIcon.shadowRoot!.querySelector('img')!.src);
  });

  test('fallbackAvatarDisplayedWhenNoSignedInUser', async function() {
    syncBrowserProxy.storedAccounts = [];

    const avatarIcon = document.createElement('settings-avatar-icon');
    document.body.appendChild(avatarIcon);
    flush();

    await syncBrowserProxy.whenCalled('getStoredAccounts');

    assertEquals(
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE',
        avatarIcon.shadowRoot!.querySelector('img')!.src);
  });
});
