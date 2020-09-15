// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://nearby/app.js';

import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.m.js';
import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitAfterNextRender} from '../test_util.m.js';

import {FakeContactManager} from './shared/fake_nearby_contact_manager.m.js';
import {FakeNearbyShareSettings} from './shared/fake_nearby_share_settings.m.js';

suite('ShareAppTest', function() {
  /** @type {!NearbyShareAppElement} */
  let shareAppElement;

  /** @param {!string} page Page to check if it is active. */
  function isPageActive(page) {
    return shareAppElement.$$(`nearby-${page}-page`)
        .classList.contains('active');
  }

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param {boolean} enabled The value of the enabled setting.
   */
  function sharedSetup(enabled) {
    /** @type {!nearbyShare.mojom.NearbyShareSettingsInterface} */
    let fakeSettings = new FakeNearbyShareSettings();
    fakeSettings.setEnabled(enabled);
    setNearbyShareSettingsForTesting(fakeSettings);

    let fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    shareAppElement = /** @type {!NearbyShareAppElement} */ (
        document.createElement('nearby-share-app'));
    document.body.appendChild(shareAppElement);
  }

  /** Shared teardown for both sub-suites. */
  function sharedTeardown() {
    shareAppElement.remove();
  }

  suite('EnabledTests', function() {
    setup(function() {
      sharedSetup(true);
    });

    teardown(sharedTeardown);

    test('renders discovery page when enabled', async function() {
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.$$('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('discovery'));
    });
  });

  suite('DisabledTests', function() {
    setup(function() {
      sharedSetup(false);
    });

    teardown(sharedTeardown);

    test('renders onboarding page when disabled', async function() {
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.$$('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('onboarding'));
    });

    test('changes page on event', async function() {
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.$$('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('onboarding'));

      shareAppElement.fire('change-page', {page: 'discovery'});

      // Discovery page should now be active, other pages should not.
      assertTrue(isPageActive('discovery'));
      assertFalse(isPageActive('onboarding'));
    });
  });
});
