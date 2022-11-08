// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {setUserActionRecorderForTesting, userActionRecorderMojomWebui} from 'chrome://os-settings/chromeos/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';

suite('InternetConfig', function() {
  /** @type {!InternetConfig|undefined} */
  let internetConfig;

  /** @type {!CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  /** @type {?userActionRecorderMojomWebui.UserActionRecorderInterface} */
  let userActionRecorder;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    internetConfig = document.createElement('internet-config');
    internetConfig.type = OncMojo.getNetworkTypeString(NetworkType.kWiFi);
    document.body.appendChild(internetConfig);
    flush();

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
  });

  test('Cancel button closes the dialog', function() {
    internetConfig.open();
    assertTrue(internetConfig.$.dialog.open);

    internetConfig.shadowRoot.querySelector('cr-button.cancel-button').click();
    assertFalse(internetConfig.$.dialog.open);
  });

  test('Connect button click increments settings change count', function() {
    internetConfig.open();
    internetConfig.showConnect = true;
    flush();

    const connectBtn =
        internetConfig.shadowRoot.querySelector('#connectButton');
    connectBtn.disabled = false;
    flush();

    assertFalse(connectBtn.disabled);
    assertEquals(userActionRecorder.settingChangeCount, 0);
    internetConfig.shadowRoot.querySelector('cr-button.action-button').click();
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });

  test('Save button click increments settings change count', function() {
    internetConfig.open();
    internetConfig.showConnect = false;
    flush();

    const saveBtn = internetConfig.shadowRoot.querySelector('#saveButton');
    saveBtn.disabled = false;
    flush();

    assertFalse(saveBtn.disabled);
    assertEquals(userActionRecorder.settingChangeCount, 0);
    internetConfig.shadowRoot.querySelector('cr-button.action-button').click();
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });
});
