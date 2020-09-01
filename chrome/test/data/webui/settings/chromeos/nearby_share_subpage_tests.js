// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {setNearbyShareSettingsForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';
// clang-format on

suite('NearbyShare', function() {
  /** @type {?SettingsNearbyShareSubpage} */
  let subpage = null;
  /** @type {?HTMLElement} */
  let onOffText = null;
  /** @type {?SettingsToggleButtonElement} */
  let featureToggleButton = null;
  /** @type {?HTMLElement} */
  let toggleRow = null;

  setup(function() {
    /** @type {!nearbyShare.mojom.NearbyShareSettingsInterface} */
    const fakeSettings = new nearby_share.FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);
    nearby_share.setNearbyShareSettingsForTesting(fakeSettings);

    PolymerTest.clearBody();
    subpage = document.createElement('settings-nearby-share-subpage');
    subpage.prefs = {
      'nearby_sharing': {
        'enabled': {
          value: true,
        },
        'data_usage': {
          value: 3,
        },
        'device_name': {
          value: '',
        }
      }
    };

    document.body.appendChild(subpage);
    Polymer.dom.flush();

    onOffText = subpage.$$('#onOff');
    featureToggleButton = subpage.$$('#featureToggleButton');
    toggleRow = subpage.$$('#toggleRow');
  });

  teardown(function() {
    subpage.remove();
  });

  test('feature toggle button controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', onOffText.textContent.trim());

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', onOffText.textContent.trim());
  });

  test('toggle row controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', onOffText.textContent.trim());

    toggleRow.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', onOffText.textContent.trim());
  });

  test('update device name preference', function() {
    assertEquals('', subpage.prefs.nearby_sharing.device_name.value);

    subpage.$$('#editDeviceNameButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-device-name-dialog');
    const newName = 'NEW NAME';
    dialog.$$('cr-input').value = newName;
    dialog.$$('.action-button').click();

    assertEquals(newName, subpage.prefs.nearby_sharing.device_name.value);
  });

  test('update visibility shows dialog', function() {
    // NOTE: all value editing is done and tested in the
    // nearby-contact-visibility component which is hosted directly on the
    // dialog. Here we just verify the dialog shows up, it has the component,
    // and it has a close/action button.
    subpage.$$('#editVisibilityButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-contact-visibility-dialog');
    assertTrue(dialog.$$('nearby-contact-visibility') !== null);
    dialog.$$('.action-button').click();
  });

  test('update data usage preference', function() {
    assertEquals(3, subpage.prefs.nearby_sharing.data_usage.value);

    subpage.$$('#editDataUsageButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-data-usage-dialog');
    dialog.$$('#dataUsageDataButton').click();
    dialog.$$('.action-button').click();

    assertEquals(2, subpage.prefs.nearby_sharing.data_usage.value);
  });

  test('show receive dialog', function() {
    subpage.showReceiveDialog_ = true;
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!dialog);
  });

});
