// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement, InternetConfigElement, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';

suite('<internet-config>', () => {
  let internetConfig: InternetConfigElement;
  let mojoApi_: FakeNetworkConfig;
  let userActionRecorder: FakeUserActionRecorder;

  suiteSetup(() => {
    mojoApi_ = new FakeNetworkConfig();
    const mojoInstance = new MojoInterfaceProviderImpl();
    mojoInstance.setMojoServiceRemoteForTest(mojoApi_);
  });

  setup(() => {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    internetConfig = document.createElement('internet-config');
    internetConfig.type = OncMojo.getNetworkTypeString(NetworkType.kWiFi);
    document.body.appendChild(internetConfig);
    flush();
  });

  teardown(() => {
    internetConfig.remove();
  });

  test('Cancel button closes the dialog', () => {
    internetConfig.open();
    assertTrue(internetConfig.$.dialog.open);

    const button = internetConfig.shadowRoot!.querySelector<CrButtonElement>(
        'cr-button.cancel-button');
    assertTrue(!!button);
    button.click();
    assertFalse(internetConfig.$.dialog.open);
  });

  test('Connect button click increments settings change count', () => {
    internetConfig.open();
    internetConfig.showConnect = true;
    flush();

    const connectBtn =
        internetConfig.shadowRoot!.querySelector<CrButtonElement>(
            '#connectButton');
    assertTrue(!!connectBtn);
    connectBtn.disabled = false;
    flush();

    assertFalse(connectBtn.disabled);
    assertEquals(0, userActionRecorder.settingChangeCount);
    const button = internetConfig.shadowRoot!.querySelector<CrButtonElement>(
        'cr-button.action-button');
    assertTrue(!!button);
    button.click();
    assertEquals(1, userActionRecorder.settingChangeCount);
  });

  test('Save button click increments settings change count', () => {
    internetConfig.open();
    internetConfig.showConnect = false;
    flush();

    const saveBtn = internetConfig.shadowRoot!.querySelector<CrButtonElement>(
        '#saveButton');
    assertTrue(!!saveBtn);
    saveBtn.disabled = false;
    flush();

    assertFalse(saveBtn.disabled);
    assertEquals(0, userActionRecorder.settingChangeCount);
    const button = internetConfig.shadowRoot!.querySelector<CrButtonElement>(
        'cr-button.action-button');
    assertTrue(!!button);
    button.click();
    assertEquals(1, userActionRecorder.settingChangeCount);
  });
});
