// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/shared/nearby_device_icon.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ShareTargetType} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom-webui.js';

import {assertEquals} from '../../chai_assert.js';

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
    const shareTarget = /** @type {!ShareTarget} */ ({
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
    testIcon(ShareTargetType.kPhone, 'nearby-share:smartphone');
  });

  test('renders tablet', function() {
    testIcon(ShareTargetType.kTablet, 'nearby-share:tablet');
  });

  test('renders laptop', function() {
    testIcon(ShareTargetType.kLaptop, 'nearby-share:laptop');
  });

  test('renders unknown as laptop', function() {
    testIcon(ShareTargetType.kUnknown, 'nearby-share:laptop');
  });
});
