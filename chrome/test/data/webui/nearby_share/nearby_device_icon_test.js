// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';

import 'chrome://nearby/nearby_device_icon.js';
import 'chrome://nearby/nearby_share.mojom-lite.js';

import {assertEquals} from '../chai_assert.js';

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
    const renderedIcon = deviceIconElement.$$('#icon').icon;
    assertEquals('nearby-share:laptop', renderedIcon);
  });

  const iconTests = [
    {
      type: nearbyShare.mojom.ShareTargetType.kPhone,
      expected: 'nearby-share:smartphone'
    },
    {
      type: nearbyShare.mojom.ShareTargetType.kTablet,
      expected: 'nearby-share:tablet'
    },
    {
      type: nearbyShare.mojom.ShareTargetType.kLaptop,
      expected: 'nearby-share:laptop'
    },
    {
      type: nearbyShare.mojom.ShareTargetType.kUnknown,
      expected: 'nearby-share:laptop'
    },
  ];

  iconTests.forEach(function({type, expected}) {
    test(`renders ${expected}`, function() {
      const shareTarget = /** @type {!nearbyShare.mojom.ShareTarget} */ ({
        id: {high: 0, low: 0},
        name: 'Device Name',
        type,
      });
      deviceIconElement.shareTarget = shareTarget;

      const renderedIcon = deviceIconElement.$$('#icon').icon;
      assertEquals(expected, renderedIcon);
    });
  });
});
