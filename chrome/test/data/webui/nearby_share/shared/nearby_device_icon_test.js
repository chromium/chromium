// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://nearby/mojo/nearby_share_target_types.mojom-lite.js';
import 'chrome://nearby/mojo/nearby_share_share_type.mojom-lite.js';
import 'chrome://nearby/mojo/nearby_share.mojom-lite.js';

import {NearbyDeviceIconElement} from 'chrome://nearby/shared/nearby_device_icon.js';

import {assertEquals} from '../../chromeos/chai_assert.js';

suite('DeviceIconTest', function() {
  /** @type {!NearbyDeviceIconElement} */
  let deviceIconElement;

  setup(function() {
    deviceIconElement = /** @type {!NearbyDeviceIconElement} */ (
        document.createElement('nearby-device-icon'));
    document.body.appendChild(deviceIconElement);
  });

  teardown(function() {
    deviceIconElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-DEVICE-ICON', deviceIconElement.tagName);
  });

  test('renders default icon', function() {
    const renderedIcon =
        deviceIconElement.shadowRoot.querySelector('#icon').icon;
    assertEquals('nearby-share:laptop', renderedIcon);
  });

  function testIcon(type, expected) {
    const shareTarget = /** @type {!nearbyShare.mojom.ShareTarget} */ ({
      id: {high: 0, low: 0},
      name: 'Device Name',
      type,
    });
    deviceIconElement.shareTarget = shareTarget;

    const renderedIcon =
        deviceIconElement.shadowRoot.querySelector('#icon').icon;
    assertEquals(expected, renderedIcon);
  }

  test('renders phone', function() {
    testIcon(nearbyShare.mojom.ShareTargetType.kPhone,
      'nearby-share:smartphone');
  });

  test('renders tablet', function() {
    testIcon(nearbyShare.mojom.ShareTargetType.kTablet, 'nearby-share:tablet');
  });

  test('renders laptop', function() {
    testIcon(nearbyShare.mojom.ShareTargetType.kLaptop, 'nearby-share:laptop');
  });

  test('renders unknown as laptop', function() {
    testIcon(nearbyShare.mojom.ShareTargetType.kUnknown, 'nearby-share:laptop');
  });
});
