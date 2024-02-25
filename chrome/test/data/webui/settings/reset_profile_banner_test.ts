// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsResetProfileBannerElement} from 'chrome://settings/settings.js';
import {ResetBrowserProxyImpl, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestResetBrowserProxy} from './test_reset_browser_proxy.js';

// clang-format on

suite('BannerTests', function() {
  let resetBanner: SettingsResetProfileBannerElement;
  let browserProxy: TestResetBrowserProxy;

  setup(function() {
    browserProxy = new TestResetBrowserProxy();
    ResetBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetBanner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(resetBanner);
    assertTrue(resetBanner.$.dialog.open);
  });

  teardown(function() {
    resetBanner.remove();
  });

  // Tests that the reset profile banner navigates to the Reset profile dialog
  // URL when the "reset all settings" button is clicked.
  test('ResetBannerReset', function() {
    assertNotEquals(
        routes.RESET_DIALOG, Router.getInstance().getCurrentRoute());
    resetBanner.$.reset.click();
    assertEquals(routes.RESET_DIALOG, Router.getInstance().getCurrentRoute());
    assertFalse(resetBanner.$.dialog.open);
  });

  // Tests that the reset profile banner closes itself when the OK button is
  // clicked and that |onHideResetProfileBanner| is called.
  test('ResetBannerOk', async function() {
    resetBanner.$.ok.click();
    await browserProxy.whenCalled('onHideResetProfileBanner');
    assertFalse(resetBanner.$.dialog.open);
  });
});
