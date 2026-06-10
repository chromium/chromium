// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsResetProfileBannerElement} from 'chrome://settings/settings.js';
import {loadTimeData, ResetBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestResetBrowserProxy} from './test_reset_browser_proxy.js';

// clang-format on

class TestResetBrowserProxyWithOpen extends TestResetBrowserProxy {
  openWindowUrl: string|null = null;
  constructor() {
    super();
    window.open = (url) => {
      if (url) {
        this.openWindowUrl = url ? url.toString() : null;
      }
      return null;
    };
  }
}

suite('ResetProfileBanner', function() {
  let banner: SettingsResetProfileBannerElement;
  let browserProxy: TestResetBrowserProxyWithOpen;

  setup(function() {
    loadTimeData.overrideValues({
      // These are mock strings for verification.
      resetAutomatedDialogTitle: 'Chrome reset these settings',
      resetAutomatedDialogBody: 'To protect you, Chrome reset them.',
      gotIt: 'Got it',
      learnMore: 'Learn more',
      resetProfileBannerLearnMoreUrl:
          'https://google.com/zackstestinglink/learnmore',
    });

    browserProxy = new TestResetBrowserProxyWithOpen();
    ResetBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    banner.remove();
  });

  test('showsBannerWithCorrectContent', async function() {
    const tamperedPrefs = ['Your search engine', 'Your pinned tabs'];
    browserProxy.setTamperedPreferencePaths(tamperedPrefs);
    banner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(banner);

    await browserProxy.whenCalled('getTamperedPreferencePaths');
    await browserProxy.whenCalled('onShowResetProfileDialog');
    flush();

    const dialog = banner.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog, 'Dialog element should be stamped');
    assertTrue(dialog.open, 'Dialog should be open');

    // Verify title and body strings.
    const title = banner.shadowRoot!.querySelector('[slot=title]');
    const body = banner.shadowRoot!.querySelector('[slot=body]');
    assertTrue(!!title);
    assertTrue(!!body);
    assertEquals('Chrome reset these settings', title.textContent.trim());
    assertTrue(body.textContent.includes('To protect you, Chrome reset them.'));

    // Verify the list of tampered preferences correctly.
    const listItems = banner.shadowRoot!.querySelectorAll('li');
    assertEquals(tamperedPrefs.length, listItems.length);
    assertEquals(tamperedPrefs[0], listItems[0]!.textContent.trim());
    assertEquals(tamperedPrefs[1], listItems[1]!.textContent.trim());

    // Verify button text.
    const learnMoreButton = banner.shadowRoot!.querySelector('#learnMore');
    const confirmButton = banner.shadowRoot!.querySelector('#confirm');
    assertTrue(!!learnMoreButton);
    assertTrue(!!confirmButton);
    assertEquals('Learn more', learnMoreButton.textContent.trim());
    assertEquals('Got it', confirmButton.textContent.trim());
  });

  test('showsNothingWhenNoPrefs', async function() {
    browserProxy.setTamperedPreferencePaths([]);
    banner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(banner);

    await browserProxy.whenCalled('getTamperedPreferencePaths');
    flush();

    const dialog = banner.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog, 'Dialog element should be stamped');
    assertFalse(dialog.open, 'Dialog should not be open');
  });

  test('confirmButtonClosesDialogAndCallsProxy', async function() {
    browserProxy.setTamperedPreferencePaths(['Search engine']);
    banner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(banner);
    await browserProxy.whenCalled('getTamperedPreferencePaths');
    flush();

    const dialog = banner.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog && dialog.open);

    const confirmButton =
        banner.shadowRoot!.querySelector<HTMLElement>('#confirm');
    assertTrue(!!confirmButton);
    confirmButton.click();

    await browserProxy.whenCalled('onHideResetProfileBanner');
    flush();
    assertFalse(dialog.open);
  });

  test('learnMoreButtonOpensNewWindow', async function() {
    browserProxy.setTamperedPreferencePaths(['Search engine']);
    banner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(banner);
    await browserProxy.whenCalled('getTamperedPreferencePaths');
    flush();

    const learnMoreButton =
        banner.shadowRoot!.querySelector<HTMLElement>('#learnMore');
    assertTrue(!!learnMoreButton);
    learnMoreButton.click();

    assertEquals(
        'https://google.com/zackstestinglink/learnmore',
        browserProxy.openWindowUrl);
  });
});
