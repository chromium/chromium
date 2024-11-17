// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {NetworkDeviceInfoDialogElement} from 'chrome://os-settings/lazy_load.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ESimManagerRemote, EuiccRemote, QRCode} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote, FakeEuicc} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeCanvasContext} from '../fake_canvas_context.js';

suite('<network-device-info-dialog>', () => {
  let eSimManagerRemote: FakeESimManagerRemote;
  let testEuicc: FakeEuicc;
  let deviceInfoDialog: NetworkDeviceInfoDialogElement;
  let canvasContext: FakeCanvasContext;
  let mojoApi: FakeNetworkConfig;

  async function init(
      eidQRCode?: QRCode, deviceState?: OncMojo.DeviceStateProperties,
      noEuicc?: boolean): Promise<void> {
    deviceInfoDialog = document.createElement('network-device-info-dialog');
    deviceInfoDialog.deviceState = deviceState;

    if (!noEuicc) {
      testEuicc = eSimManagerRemote.addEuiccForTest(1);
      deviceInfoDialog.euicc = testEuicc as unknown as EuiccRemote;
    }

    if (eidQRCode) {
      assertFalse(!!noEuicc);
      testEuicc.setEidQRCodeForTest(eidQRCode);
    }

    canvasContext = new FakeCanvasContext();
    deviceInfoDialog.setCanvasContextForTest(
        canvasContext as unknown as CanvasRenderingContext2D);
    document.body.appendChild(deviceInfoDialog);

    await flushTasks();
  }

  setup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    mojoApi.resetForTest();
    mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);
  });

  test('Should display EID', async () => {
    await init();
    const eidElem = deviceInfoDialog.shadowRoot!.querySelector('#eid');
    assertTrue(!!eidElem);
    assertEquals(testEuicc.properties.eid, eidElem.textContent?.trim());
    assertNull(deviceInfoDialog.shadowRoot!.querySelector('#imei'));
    const ariaElem = deviceInfoDialog.shadowRoot!.querySelector('#body');
    assertTrue(!!ariaElem);
    assertEquals(
        deviceInfoDialog.i18n(
            'deviceInfoPopupA11yEid', testEuicc.properties.eid),
        ariaElem.ariaLabel);

  });

  test('Should render EID QRCode', async () => {
    await init({size: 2, data: [1, 0, 0, 1]});
    const qrCodeCanvas =
        deviceInfoDialog.shadowRoot!.querySelector<HTMLCanvasElement>(
            '#qrCodeCanvas');
    assertTrue(!!qrCodeCanvas);
    assertEquals(50, qrCodeCanvas.width);
    assertEquals(50, qrCodeCanvas.height);
    assertDeepEquals([[0, 0, 50, 50]], canvasContext.getClearRectCalls());
    assertDeepEquals(
        [[20, 20, 5, 5], [25, 25, 5, 5]], canvasContext.getFillRectCalls());
  });

  test('Should display IMEI', async () => {
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.imei = '1234567890';
    await init(undefined, cellularDeviceState, true);
    const imeiElem = deviceInfoDialog.shadowRoot!.querySelector('#imei');
    assertTrue(!!imeiElem);
    assertEquals(cellularDeviceState.imei, imeiElem.textContent?.trim());
    assertNull(deviceInfoDialog.shadowRoot!.querySelector('#eid'));
    const ariaElem = deviceInfoDialog.shadowRoot!.querySelector('#body');
    assertTrue(!!ariaElem);
    assertEquals(
        deviceInfoDialog.i18n(
            'deviceInfoPopupA11yImei', cellularDeviceState.imei),
        ariaElem.ariaLabel);
  });

  test('Should display serial', async () => {
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.serial = 'ABCD';
    await init(undefined, cellularDeviceState, true);
    const serialElem =
        deviceInfoDialog.shadowRoot!.querySelector('#serialLabel');
    assertTrue(!!serialElem);
    assertEquals(cellularDeviceState.serial, serialElem.textContent?.trim());
    const ariaElem = deviceInfoDialog.shadowRoot!.querySelector('#body');
    assertTrue(!!ariaElem);
    assertEquals(
        deviceInfoDialog.i18n(
            'deviceInfoPopupA11ySerial', cellularDeviceState.serial),
        ariaElem.ariaLabel);
  });

  test('Should not display serial if not set', async () => {
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    await init(undefined, cellularDeviceState, true);
    const serialElem =
        deviceInfoDialog.shadowRoot!.querySelector('#serialLabel');
    assertNull(serialElem);
  });

  test('Should display both EID and IMEI', async () => {
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.imei = '1234567890';
    await init(undefined, cellularDeviceState);
    assertTrue(!!deviceInfoDialog.shadowRoot!.querySelector('#imei'));
    assertTrue(!!deviceInfoDialog.shadowRoot!.querySelector('#eid'));
    const ariaElem = deviceInfoDialog.shadowRoot!.querySelector('#body');
    assertTrue(!!ariaElem);
    assertEquals(
        deviceInfoDialog.i18n(
            'deviceInfoPopupA11yEidAndImei', testEuicc.properties.eid,
            cellularDeviceState.imei),
        ariaElem.ariaLabel);
  });

  test('Test aria label with all set', async () => {
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.imei = '1234567890';
    cellularDeviceState.serial = 'ABCD';
    await init(undefined, cellularDeviceState);
    assertTrue(!!deviceInfoDialog.shadowRoot!.querySelector('#imei'));
    assertTrue(!!deviceInfoDialog.shadowRoot!.querySelector('#eid'));
    const serialElem =
        deviceInfoDialog.shadowRoot!.querySelector('#serialLabel');
    assertTrue(!!serialElem);
    assertEquals(cellularDeviceState.serial, serialElem.textContent?.trim());
    const ariaElem = deviceInfoDialog.shadowRoot!.querySelector('#body');
    assertTrue(!!ariaElem);
    assertEquals(
        deviceInfoDialog.i18n(
            'deviceInfoPopupA11yEidImeiAndSerial', testEuicc.properties.eid,
            cellularDeviceState.imei, cellularDeviceState.serial),
        ariaElem.ariaLabel);
  });
});
