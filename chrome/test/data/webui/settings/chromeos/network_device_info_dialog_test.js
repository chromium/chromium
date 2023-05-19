// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';

import {FakeCanvasContext} from './fake_canvas_context.js';

suite('NetworkDeviceInfoDialog', function() {
  let eSimManagerRemote;
  let testEuicc;
  let deviceInfoDialog;
  let canvasContext;
  let mojoApi_;

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  async function init(eidQRCode, deviceState, noEuicc) {
    deviceInfoDialog = document.createElement('network-device-info-dialog');
    deviceInfoDialog.deviceState = deviceState;

    if (!noEuicc) {
      testEuicc = eSimManagerRemote.addEuiccForTest(1);
      deviceInfoDialog.euicc = testEuicc;
    }

    if (eidQRCode) {
      assertFalse(!!noEuicc);
      testEuicc.setEidQRCodeForTest(eidQRCode);
    }

    canvasContext = new FakeCanvasContext();
    deviceInfoDialog.setCanvasContextForTest(canvasContext);
    document.body.appendChild(deviceInfoDialog);

    await flushAsync();
  }

  setup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
  });

  test('Should display EID', async function() {
    await init();
    const eidElem = deviceInfoDialog.shadowRoot.querySelector('#eid');
    assertEquals(testEuicc.properties.eid, eidElem.textContent.trim());
    assertEquals(null, deviceInfoDialog.shadowRoot.querySelector('#imei'));
  });

  test('Should render EID QRCode', async function() {
    await init({size: 2, data: [1, 0, 0, 1]});
    const qrCodeCanvas =
        deviceInfoDialog.shadowRoot.querySelector('#qrCodeCanvas');
    assertEquals(qrCodeCanvas.width, 10);
    assertEquals(qrCodeCanvas.height, 10);
    assertDeepEquals(canvasContext.getClearRectCalls(), [[0, 0, 10, 10]]);
    assertDeepEquals(
        canvasContext.getFillRectCalls(), [[0, 0, 5, 5], [5, 5, 5, 5]]);
  });

  test('Should display IMEI', async function() {
    const cellularDeviceState =
        mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    cellularDeviceState.imei = '1234567890';
    await init(null, cellularDeviceState, true);
    const imeiElem = deviceInfoDialog.shadowRoot.querySelector('#imei');
    assertEquals(cellularDeviceState.imei, imeiElem.textContent.trim());
    assertEquals(null, deviceInfoDialog.shadowRoot.querySelector('#eid'));
  });

  test('Should display both EID and IMEI', async function() {
    const cellularDeviceState =
        mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    cellularDeviceState.imei = '1234567890';
    await init(null, cellularDeviceState);
    assertNotEquals(null, deviceInfoDialog.shadowRoot.querySelector('#imei'));
    assertNotEquals(null, deviceInfoDialog.shadowRoot.querySelector('#eid'));
  });
});