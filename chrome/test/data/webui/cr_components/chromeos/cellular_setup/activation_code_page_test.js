// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/activation_code_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeMediaDevices} from './fake_media_devices.m.js';
// #import {FakeBarcodeDetector} from './fake_barcode_detector.m.js';
// clang-format on

suite('CrComponentsActivationCodePageTest', function() {
  let activationCodePage;

  /** @type {?FakeMediaDevices} */
  let mediaDevices = null;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    activationCodePage = document.createElement('activation-code-page');
    activationCodePage.barcodeDetectorClass_ = FakeBarcodeDetector;
    activationCodePage.initBarcodeDetector();
    document.body.appendChild(activationCodePage);
    Polymer.dom.flush();

    mediaDevices = new cellular_setup.FakeMediaDevices();
    mediaDevices.addDevice();
    activationCodePage.setMediaDevices(mediaDevices);
    Polymer.dom.flush();
  });

  test('UI states', async function() {
    await flushAsync();
    let qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');
    const activationCodeContainer =
        activationCodePage.$$('#activationCodeContainer');
    const video = activationCodePage.$$('#video');
    const startScanningContainer =
        activationCodePage.$$('#startScanningContainer');
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const scanFinishContainer = activationCodePage.$$('#scanFinishContainer');
    const switchCameraButton = activationCodePage.$$('#switchCameraButton');
    const scanSuccessContainer = activationCodePage.$$('#scanSuccessContainer');
    const scanFailureContainer = activationCodePage.$$('#scanFailureContainer');
    const spinner = activationCodePage.$$('paper-spinner-lite');

    assertTrue(!!qrCodeDetectorContainer);
    assertTrue(!!activationCodeContainer);
    assertTrue(!!video);
    assertTrue(!!startScanningContainer);
    assertTrue(!!startScanningButton);
    assertTrue(!!scanFinishContainer);
    assertTrue(!!switchCameraButton);
    assertTrue(!!scanSuccessContainer);
    assertTrue(!!scanFailureContainer);
    assertTrue(!!spinner);

    // Initial state should only be showing the start scanning UI.
    assertFalse(startScanningContainer.hidden);
    assertFalse(activationCodeContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);
    assertTrue(spinner.hidden);

    // Click the start scanning button.
    startScanningButton.click();
    await flushAsync();

    // The video should be visible and start scanning UI hidden.
    assertFalse(video.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    // Mock detecting an activation code.
    activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';
    await flushAsync();

    // The scanFinishContainer and scanSuccessContainer should now be visible,
    // video, start scanning UI and scanFailureContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertFalse(scanSuccessContainer.hidden);
    assertTrue(scanFailureContainer.hidden);

    // Mock an invalid activation code.
    activationCodePage.showError = true;

    // The scanFinishContainer and scanFailureContainer should now be visible,
    // video, start scanning UI and scanSuccessContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanSuccessContainer.hidden);
    assertFalse(scanFailureContainer.hidden);

    // Enter a new activation code
    activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE 2';
    await flushAsync();

    // The scanFinishContainer and scanSuccessContainer should now be visible,
    // video, start scanning UI and scanFailureContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertFalse(scanSuccessContainer.hidden);
    assertTrue(scanFailureContainer.hidden);
    assertFalse(activationCodePage.showError);

    // Mock another invalid activation code.
    activationCodePage.showError = true;

    // The scanFinishContainer and scanFailureContainer should now be visible,
    // video, start scanning UI and scanSuccessContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanSuccessContainer.hidden);
    assertFalse(scanFailureContainer.hidden);

    activationCodePage.showBusy = true;
    assertFalse(spinner.hidden);

    // Mock, no media devices present
    mediaDevices.removeDevice();
    await flushAsync();

    // When no camera device is present qrCodeDetector container should
    // not be shown
    qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');

    assertFalse(!!qrCodeDetectorContainer);
  });

  test('Switch camera button states', async function() {
    await flushAsync();
    const video = activationCodePage.$$('#video');
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const switchCameraButton = activationCodePage.$$('#switchCameraButton');

    assertTrue(!!video);
    assertTrue(!!startScanningButton);
    assertTrue(!!switchCameraButton);

    // Initial state should only be showing the start scanning UI.
    assertTrue(video.hidden);
    assertTrue(switchCameraButton.hidden);

    // Click the start scanning button.
    startScanningButton.click();
    await flushAsync();

    // The video should be visible and switch camera button hidden.
    assertFalse(video.hidden);
    assertTrue(switchCameraButton.hidden);
    assertTrue(mediaDevices.isStreamingUserFacingCamera);

    // Add a new video device.
    mediaDevices.addDevice();
    await flushAsync();

    // The switch camera button should now be visible.
    assertFalse(switchCameraButton.hidden);
    assertTrue(mediaDevices.isStreamingUserFacingCamera);

    switchCameraButton.click();
    await flushAsync();

    // The second device should now be streaming.
    assertFalse(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Switch back.
    switchCameraButton.click();
    await flushAsync();

    // The first device should be streaming again.
    assertTrue(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Switch to the second device again.
    switchCameraButton.click();
    await flushAsync();

    assertFalse(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Disconnect the second device.
    mediaDevices.removeDevice();
    await flushAsync();

    // The first device should now be streaming and the switch camera button
    // hidden.
    assertTrue(mediaDevices.isStreamingUserFacingCamera);
    assertTrue(switchCameraButton.hidden);

    // Mock detecting an activation code.
    activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';
    Polymer.dom.flush();

    assertTrue(video.hidden);
  });

  test('UI is disabled when showBusy property is set', async function() {
    await flushAsync();
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const switchCameraButton = activationCodePage.$$('#switchCameraButton');
    const useCameraAgainButton = activationCodePage.$$('#useCameraAgainButton');
    const tryAgainButton = activationCodePage.$$('#tryAgainButton');
    const input = activationCodePage.$$('#activationCode');

    assertTrue(!!startScanningButton);
    assertTrue(!!switchCameraButton);
    assertTrue(!!useCameraAgainButton);
    assertTrue(!!tryAgainButton);
    assertTrue(!!input);

    assertFalse(startScanningButton.disabled);
    assertFalse(switchCameraButton.disabled);
    assertFalse(useCameraAgainButton.disabled);
    assertFalse(tryAgainButton.disabled);
    assertFalse(input.disabled);

    activationCodePage.showBusy = true;

    assertTrue(startScanningButton.disabled);
    assertTrue(switchCameraButton.disabled);
    assertTrue(useCameraAgainButton.disabled);
    assertTrue(tryAgainButton.disabled);
    assertTrue(input.disabled);
  });

  test(
      'Do not show qrContainer when BarcodeDetector is not ready',
      async function() {
        await flushAsync();
        let qrCodeDetectorContainer =
            activationCodePage.$$('#esimQrCodeDetection');

        assertTrue(!!qrCodeDetectorContainer);

        FakeBarcodeDetector.setShouldFail(true);
        activationCodePage.initBarcodeDetector();

        await flushAsync();

        qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');

        assertFalse(!!qrCodeDetectorContainer);
      });
});
