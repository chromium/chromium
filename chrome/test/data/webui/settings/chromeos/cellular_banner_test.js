// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js'
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertTrue} from '../../chai_assert.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CellularBanner', function() {
  let cellularBanner;
  setup(function() {
    cellularBanner = document.createElement('cellular-banner');

    cellularBanner.deviceState = {
      type: chromeos.networkConfig.mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      inhibitReason:
          chromeos.networkConfig.mojom.InhibitReason.kInstallingProfile
    };

    assertTrue(!!cellularBanner);
    document.body.appendChild(cellularBanner);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const message = cellularBanner.i18n('cellularNetworkInstallingProfile');
    const bannerMessage = cellularBanner.$.bannerMessage;
    assertTrue(!!bannerMessage);
    assertEquals(bannerMessage.textContent.trim(), message);
  });
});