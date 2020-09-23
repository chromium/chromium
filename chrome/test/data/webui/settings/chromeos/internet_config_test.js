// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('InternetConfig', function() {
  /** @type {!InternetConfig|undefined} */
  let internetConfig;

  /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    internetConfig = document.createElement('internet-config');
    internetConfig.type = OncMojo.getNetworkTypeString(
        chromeos.networkConfig.mojom.NetworkType.kWiFi);
    document.body.appendChild(internetConfig);
    Polymer.dom.flush();
  });

  test('Cancel button closes the dialog', function() {
    internetConfig.open();
    assertTrue(internetConfig.$.dialog.open);

    internetConfig.$$('cr-button.cancel-button').click();
    assertFalse(internetConfig.$.dialog.open);
  });
});
