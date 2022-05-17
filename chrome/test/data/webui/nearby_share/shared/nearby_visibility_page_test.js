// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/strings.m.js';
import 'chrome://nearby/shared/nearby_visibility_page.js';

import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {isChildVisible, waitAfterNextRender} from '../../test_util.js';

import {FakeNearbyShareSettings} from './fake_nearby_share_settings.js';

suite('nearby-visibility-page', function() {
  /** @type {!NearbyVisibilityPageElement} */
  let visibility_page;

  setup(function() {
    document.body.innerHTML = '';

    visibility_page = /** @type {!NearbyVisibilityPageElement} */ (
        document.createElement('nearby-visibility-page'));
    visibility_page.settings = {
      enabled: false,
      fastInitiationNotificationState:
          nearbyShare.mojom.FastInitiationNotificationState.kEnabled,
      isFastInitiationHardwareSupported: true,
      deviceName: 'deviceName',
      dataUsage: nearbyShare.mojom.DataUsage.kOnline,
      visibility: nearbyShare.mojom.Visibility.kAllContacts,
      isOnboardingComplete: false,
      allowedContacts: [],
    };
    document.body.appendChild(visibility_page);
  });

  test('Renders visibility page', async function() {
    assertFalse(visibility_page.settings.enabled);
    await waitAfterNextRender(visibility_page);
    // Action button on the page template sets settings.enabled to true.
    const page_template =
        visibility_page.shadowRoot.querySelector('nearby-page-template');
    page_template.shadowRoot.querySelector('#actionButton').click();
    assertTrue(visibility_page.settings.enabled);
  });
});
