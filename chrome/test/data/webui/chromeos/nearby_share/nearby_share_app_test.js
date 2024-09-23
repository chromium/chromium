// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/app.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.js';
import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {FakeContactManager} from './shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from './shared/fake_nearby_share_settings.js';

suite('ShareAppTest', function() {
  /** @type {!NearbyShareAppElement} */
  let shareAppElement;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings;

  /** @param {!string} page Page to check if it is active. */
  function isPageActive(page) {
    return shareAppElement.shadowRoot.querySelector(`nearby-${page}-page`)
        .classList.contains('active');
  }

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param {boolean} enabled The value of the enabled setting.
   * @param {boolean} isOnboardingComplete The value of the onboarding
   *     completion state.
   */
  function sharedSetup(enabled, isOnboardingComplete) {
    fakeSettings = new FakeNearbyShareSettings();
    fakeSettings.setIsOnboardingComplete(!!isOnboardingComplete);
    fakeSettings.setEnabled(enabled);
    setNearbyShareSettingsForTesting(fakeSettings);

    const fakeContactManager = new FakeContactManager();
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
    teardown(sharedTeardown);

    test('renders discovery page when enabled', async function() {
      sharedSetup(/*enabled=*/ true, /*isOnboardingComplete=*/ true);

      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.shadowRoot.querySelector('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('discovery'));
    });
  });

  suite('DisabledTests', function() {
    teardown(sharedTeardown);

    test(
        'enables feature and opens discovery if onboarding is complete',
        async function() {
          sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ true);
          assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
          assertEquals(
              null, shareAppElement.shadowRoot.querySelector('.active'));
          // We have to wait for settings to return from the mojo after which
          // the app will route to the correct page.
          await waitAfterNextRender(shareAppElement);
          const enabledResponse = await fakeSettings.getEnabled();
          assertTrue(enabledResponse && enabledResponse.enabled);
          assertTrue(isPageActive('discovery'));
        });

    test('renders onboarding page when disabled', async function() {
      sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ false);
      loadTimeData.overrideValues({
        'isOnePageOnboardingEnabled': false,
      });
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.shadowRoot.querySelector('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('onboarding'));
    });

    test('renders one-page onboarding page when disabled', async function() {
      sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ false);
      loadTimeData.overrideValues({
        'isOnePageOnboardingEnabled': true,
      });
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.shadowRoot.querySelector('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('onboarding-one'));
    });

    test('changes page on event', async function() {
      sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ false);
      assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
      assertEquals(null, shareAppElement.shadowRoot.querySelector('.active'));
      // We have to wait for settings to return from the mojo after which
      // the app will route to the correct page.
      await waitAfterNextRender(shareAppElement);
      assertTrue(isPageActive('onboarding-one'));

      shareAppElement.dispatchEvent(new CustomEvent(
          'change-page',
          {bubbles: true, composed: true, detail: {page: 'discovery'}}));

      // Discovery page should now be active, other pages should not.
      assertTrue(isPageActive('discovery'));
      assertFalse(isPageActive('onboarding-one'));
    });
  });
});
