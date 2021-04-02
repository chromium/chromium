// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/activation_code_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeMediaDevices} from './fake_media_devices.m.js';
// #import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.m.js';
// clang-format on

suite('CrComponentsActivationCodePageTest', function() {
  let activationCodePage;

  /** @type {?FakeMediaDevices} */
  let mediaDevices = null;

  /** @type {function(Function, number)} */
  let intervalFunction = null;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  // Captures the function that is called every time the interval timer
  // timeouts.
  function setIntervalFunction(fn, milliseconds) {
    intervalFunction = fn;
    return 1;
  }

  // In tests, pausing the video can have race conditions with previous
  // requests to play the video due to the speed of execution. Avoid this by
  // mocking the play and pause actions.
  function playVideoFunction() {}
  function stopStreamFunction(stream) {}

  setup(async function() {
    activationCodePage = document.createElement('activation-code-page');
    await activationCodePage.setFakesForTesting(
        FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
        playVideoFunction, stopStreamFunction);
    document.body.appendChild(activationCodePage);
    Polymer.dom.flush();

    mediaDevices = new cellular_setup.FakeMediaDevices();
    mediaDevices.addDevice();
    activationCodePage.setMediaDevices(mediaDevices);
    Polymer.dom.flush();
  });

  teardown(function() {
    activationCodePage.remove();
    FakeBarcodeDetector.setShouldFail(false);
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
    const useCameraAgainButton = activationCodePage.$$('#useCameraAgainButton');
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
    assertTrue(!!useCameraAgainButton);
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

    // Mock camera scanning a code.
    await intervalFunction();
    await flushAsync();

    // The scanFinishContainer and scanSuccessContainer should now be visible,
    // video, start scanning UI and scanFailureContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertFalse(scanSuccessContainer.hidden);
    assertTrue(scanFailureContainer.hidden);

    // Click the 'Use camera again' button.
    useCameraAgainButton.click();
    await flushAsync();

    // The video should be visible and start scanning UI hidden.
    assertFalse(video.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    // Scan a new code.
    await intervalFunction();
    await flushAsync();

    // The scanFinishContainer and scanSuccessContainer should now be visible,
    // video, start scanning UI and scanFailureContainer hidden.
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertFalse(scanSuccessContainer.hidden);
    assertTrue(scanFailureContainer.hidden);
    assertFalse(activationCodePage.showError);

    // Click the 'Use camera again' button.
    useCameraAgainButton.click();
    await flushAsync();

    // The video should be visible and start scanning UI hidden.
    assertFalse(video.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    // Simulate typing in the input.
    activationCodePage.$$('#activationCode')
        .dispatchEvent(new KeyboardEvent('keydown', {key: 'A'}));
    await flushAsync();

    // We should be back in the initial state.
    assertFalse(startScanningContainer.hidden);
    assertFalse(activationCodeContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);
    assertTrue(spinner.hidden);

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
    await intervalFunction();
    await flushAsync();

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
        // Activation code input should be at the bottom of the page.
        assertTrue(activationCodePage.$$('#activationCodeContainer')
                       .classList.contains('relative'));

        FakeBarcodeDetector.setShouldFail(true);
        await activationCodePage.setFakesForTesting(
            FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
            playVideoFunction, stopStreamFunction);

        qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');

        assertFalse(!!qrCodeDetectorContainer);
        // Activation code input should now be in the center of the page.
        assertTrue(activationCodePage.$$('#activationCodeContainer')
                       .classList.contains('center'));
      });

  test('Event is fired when enter is pressed on input', async function() {
    await flushAsync();
    let eventFired = false;
    activationCodePage.addEventListener('forward-navigation-requested', () => {
      eventFired = true;
    });
    const input = activationCodePage.$$('#activationCode');
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await flushAsync();
    assertTrue(eventFired);
  });

  test(
      'Install error after manual entry should show error on input',
      async function() {
        await flushAsync();
        const input = activationCodePage.$$('#activationCode');
        const startScanningContainer =
            activationCodePage.$$('#startScanningContainer');
        const scanFinishContainer =
            activationCodePage.$$('#scanFinishContainer');
        assertTrue(!!input);
        assertTrue(!!startScanningContainer);
        assertTrue(!!scanFinishContainer);
        assertFalse(input.invalid);

        input.value = 'ACTIVATION_CODE';
        activationCodePage.showError = true;
        assertTrue(input.invalid);

        // Should be showing the start scanning UI.
        assertFalse(startScanningContainer.hidden);
        assertTrue(scanFinishContainer.hidden);
      });

  test(
      'Install error after scanning should show error on camera',
      async function() {
        await flushAsync();
        const input = activationCodePage.$$('#activationCode');
        const startScanningContainer =
            activationCodePage.$$('#startScanningContainer');
        const startScanningButton =
            activationCodePage.$$('#startScanningButton');
        const scanFinishContainer =
            activationCodePage.$$('#scanFinishContainer');
        const scanInstallFailureHeader =
            activationCodePage.$$('#scanInstallFailureHeader');
        const scanSucessHeader = activationCodePage.$$('#scanSucessHeader');
        assertTrue(!!input);
        assertTrue(!!startScanningContainer);
        assertTrue(!!startScanningButton);
        assertTrue(!!scanFinishContainer);
        assertTrue(!!scanInstallFailureHeader);
        assertTrue(!!scanSucessHeader);
        assertFalse(input.invalid);

        // Click the start scanning button.
        startScanningButton.click();
        await flushAsync();

        // Mock camera scanning a code.
        await intervalFunction();
        await flushAsync();

        // The code detected UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertFalse(scanSucessHeader.hidden);
        assertTrue(scanInstallFailureHeader.hidden);

        // Mock an install error.
        activationCodePage.showError = true;

        // The scan install failure UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertTrue(scanSucessHeader.hidden);
        assertFalse(scanInstallFailureHeader.hidden);

        // There should be no error displayed on the input.
        assertFalse(input.invalid);
      });
});
