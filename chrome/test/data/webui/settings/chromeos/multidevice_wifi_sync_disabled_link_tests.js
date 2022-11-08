// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Multidevice', function() {
  let localizedLink = null;

  setup(function() {
    PolymerTest.clearBody();
    localizedLink =
        document.createElement('settings-multidevice-wifi-sync-disabled-link');
    document.body.appendChild(localizedLink);
    flush();
  });

  teardown(function() {
    localizedLink.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Contains 2 links with aria-labels', async () => {
    const chromeSyncLink =
        localizedLink.shadowRoot.querySelector('#chromeSyncLink');
    assertTrue(!!chromeSyncLink);
    assertTrue(chromeSyncLink.hasAttribute('aria-label'));
    const learnMoreLink =
        localizedLink.shadowRoot.querySelector('#learnMoreLink');
    assertTrue(!!learnMoreLink);
    assertTrue(learnMoreLink.hasAttribute('aria-label'));
  });

  test('Spans are aria-hidden', async () => {
    const spans = localizedLink.shadowRoot.querySelectorAll('span');
    spans.forEach((span) => {
      assertTrue(span.hasAttribute('aria-hidden'));
    });
  });

  test('ChromeSyncLink navigates to appropriate route', async () => {
    const chromeSyncLink =
        localizedLink.shadowRoot.querySelector('#chromeSyncLink');
    chromeSyncLink.click();
    flush();

    assertEquals(Router.getInstance().getCurrentRoute(), routes.OS_SYNC);
  });
});
