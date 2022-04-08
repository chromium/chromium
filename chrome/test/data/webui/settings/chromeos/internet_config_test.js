// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {setUserActionRecorderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';

suite('InternetConfig', function() {
  /** @type {!InternetConfig|undefined} */
  let internetConfig;

  /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  /** @type {?chromeos.settings.mojom.UserActionRecorderInterface} */
  let userActionRecorder;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    internetConfig = document.createElement('internet-config');
    internetConfig.type = OncMojo.getNetworkTypeString(
        chromeos.networkConfig.mojom.NetworkType.kWiFi);
    document.body.appendChild(internetConfig);
    flush();

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
  });

  test('Cancel button closes the dialog', function() {
    internetConfig.open();
    assertTrue(internetConfig.$.dialog.open);

    internetConfig.$$('cr-button.cancel-button').click();
    assertFalse(internetConfig.$.dialog.open);
  });

  test('Connect button click increments settings change count', function() {
    internetConfig.open();
    internetConfig.showConnect = true;
    flush();

    const connectBtn = internetConfig.$$('#connectButton');
    connectBtn.disabled = false;
    flush();

    assertFalse(connectBtn.disabled);
    assertEquals(userActionRecorder.settingChangeCount, 0);
    internetConfig.$$('cr-button.action-button').click();
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });

  test('Save button click increments settings change count', function() {
    internetConfig.open();
    internetConfig.showConnect = false;
    flush();

    const saveBtn = internetConfig.$$('#saveButton');
    saveBtn.disabled = false;
    flush();

    assertFalse(saveBtn.disabled);
    assertEquals(userActionRecorder.settingChangeCount, 0);
    internetConfig.$$('cr-button.action-button').click();
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });
});
