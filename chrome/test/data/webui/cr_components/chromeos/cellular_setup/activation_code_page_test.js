// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/activation_code_page.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.js';
import {FakeMediaDevices} from './fake_media_devices.js';

suite('CrComponentsActivationCodePageTest', function() {
  /** @type {string} */
  const ACTIVATION_CODE_VALID = 'LPA:1$ACTIVATION_CODE';

  /** @type {string} */
  const ACTIVATION_CODE_INVALID = 'INVALID';

  let activationCodePage;

  /** @type {?FakeMediaDevices} */
  let mediaDevices = null;

  /** @type {function(Function, number)} */
  let intervalFunction = null;

  function flushAsync() {
    flush();
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
    await flushAsync();

    mediaDevices = new FakeMediaDevices();
    activationCodePage.setMediaDevices(mediaDevices);
    await flushAsync();

    await addMediaDevice();
  });

  teardown(function() {
    activationCodePage.remove();
    FakeBarcodeDetector.setShouldFail(false);
  });

  async function addMediaDevice() {
    mediaDevices.addDevice();
    await flushAsync();

    await resolveEnumeratedDevicesPromise();
  }

  async function resolveEnumeratedDevicesPromise() {
    let resolver;
    const enumerateDeviceResolvedPromise = new Promise((resolve, reject) => {
      resolver = resolve;
    });
    mediaDevices.resolveEnumerateDevices(function() {
      resolver();
    });
    await enumerateDeviceResolvedPromise;
  }

  test('Page description', async function () {
    const description = activationCodePage.$$('#description');
    assertTrue(!!description);

    // Mock camera on
    activationCodePage.showNoProfilesFound = true;
    assertEquals(description.innerText.trim(),
      loadTimeData.getString('scanQRCodeNoProfilesFound'));
    activationCodePage.showNoProfilesFound = false;
    assertEquals(description.innerText.trim(),
      loadTimeData.getString('scanQRCode'));

    // Clearing devices to test without camera
    mediaDevices.removeDevice();
    await resolveEnumeratedDevicesPromise();
    activationCodePage.showNoProfilesFound = true;
    assertEquals(description.innerText.trim(),
      loadTimeData.getString('enterActivationCodeNoProfilesFound'));
    activationCodePage.showNoProfilesFound = false;
    assertEquals(description.innerText.trim(),
      loadTimeData.getString('enterActivationCode'));
  });

  test('UI states', async function() {
    let qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');
    const activationCodeContainer =
        activationCodePage.$$('#activationCodeContainer');
    const video = activationCodePage.$$('#video');
    const startScanningContainer =
        activationCodePage.$$('#startScanningContainer');
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const scanFinishContainer = activationCodePage.$$('#scanFinishContainer');
    const switchCameraButton = activationCodePage.$$('#switchCameraButton');
    const getUseCameraAgainButton = () => {
      return activationCodePage.$$('#useCameraAgainButton');
    };
    const scanSuccessContainer = activationCodePage.$$('#scanSuccessContainer');
    const scanFailureContainer = activationCodePage.$$('#scanFailureContainer');

    assertTrue(!!qrCodeDetectorContainer);
    assertTrue(!!activationCodeContainer);
    assertTrue(!!video);
    assertTrue(!!startScanningContainer);
    assertTrue(!!startScanningButton);
    assertTrue(!!scanFinishContainer);
    assertTrue(!!switchCameraButton);
    assertFalse(!!getUseCameraAgainButton());
    assertTrue(!!scanSuccessContainer);
    assertTrue(!!scanFailureContainer);
    assertFalse(!!activationCodePage.$$('paper-spinner-lite'));

    // Initial state should only be showing the start scanning UI.
    assertFalse(startScanningContainer.hidden);
    assertFalse(activationCodeContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);
    assertFalse(!!activationCodePage.$$('paper-spinner-lite'));

    // Click the start scanning button.
    startScanningButton.click();
    mediaDevices.resolveGetUserMedia();
    await flushAsync();

    // The video should be visible and start scanning UI hidden.
    assertFalse(video.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    const focusNextButtonPromise =
        eventToPromise('focus-default-button', activationCodePage);

    // Mock camera scanning a code.
    await intervalFunction();
    await flushAsync();

    // The scanFinishContainer and scanSuccessContainer should now be visible,
    // video, start scanning UI, scanFailureContainer hidden and nextbutton
    // is focused.
    await Promise.all([focusNextButtonPromise, flushTasks()]);
    assertFalse(scanFinishContainer.hidden);
    assertTrue(startScanningContainer.hidden);
    assertTrue(video.hidden);
    assertFalse(scanSuccessContainer.hidden);
    assertTrue(scanFailureContainer.hidden);
    assertFalse(!!getUseCameraAgainButton());
    assertFalse(activationCodePage.showError);

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
    assertFalse(!!activationCodePage.$$('paper-spinner-lite'));

    activationCodePage.showBusy = true;
    await flushAsync();
    assertTrue(!!activationCodePage.$$('paper-spinner-lite'));

    // Mock, no media devices present
    mediaDevices.removeDevice();
    await resolveEnumeratedDevicesPromise();

    // When no camera device is present qrCodeDetector container should
    // not be shown
    qrCodeDetectorContainer = activationCodePage.$$('#esimQrCodeDetection');

    assertFalse(!!qrCodeDetectorContainer);
  });

  test('Switch camera button states', async function() {
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
    mediaDevices.resolveGetUserMedia();
    await flushAsync();

    // The video should be visible and switch camera button hidden.
    assertFalse(video.hidden);
    assertTrue(switchCameraButton.hidden);
    assertTrue(mediaDevices.isStreamingUserFacingCamera);

    // Add a new video device.
    await addMediaDevice();

    // The switch camera button should now be visible.
    assertFalse(switchCameraButton.hidden);
    assertTrue(mediaDevices.isStreamingUserFacingCamera);

    switchCameraButton.click();
    mediaDevices.resolveGetUserMedia();
    await flushAsync();

    // The second device should now be streaming.
    assertFalse(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Switch back.
    switchCameraButton.click();
    mediaDevices.resolveGetUserMedia();
    await flushAsync();

    // The first device should be streaming again.
    assertTrue(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Switch to the second device again.
    switchCameraButton.click();
    mediaDevices.resolveGetUserMedia();
    await flushAsync();

    assertFalse(mediaDevices.isStreamingUserFacingCamera);
    assertFalse(switchCameraButton.hidden);

    // Disconnect the second device.
    mediaDevices.removeDevice();
    await resolveEnumeratedDevicesPromise();

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
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const switchCameraButton = activationCodePage.$$('#switchCameraButton');
    const tryAgainButton = activationCodePage.$$('#tryAgainButton');
    const input = activationCodePage.$$('#activationCode');

    assertTrue(!!startScanningButton);
    assertTrue(!!switchCameraButton);
    assertTrue(!!tryAgainButton);
    assertTrue(!!input);

    assertFalse(startScanningButton.disabled);
    assertFalse(switchCameraButton.disabled);
    assertFalse(tryAgainButton.disabled);
    assertFalse(input.disabled);

    activationCodePage.showBusy = true;

    assertTrue(startScanningButton.disabled);
    assertTrue(switchCameraButton.disabled);
    assertTrue(tryAgainButton.disabled);
    assertTrue(input.disabled);
  });

  test(
      'Do not show qrContainer when BarcodeDetector is not ready',
      async function() {
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
        const input = activationCodePage.$$('#activationCode');
        const startScanningContainer =
            activationCodePage.$$('#startScanningContainer');
        const scanFinishContainer =
            activationCodePage.$$('#scanFinishContainer');
        assertTrue(!!input);
        assertTrue(!!startScanningContainer);
        assertTrue(!!scanFinishContainer);
        assertFalse(input.invalid);

        input.value = ACTIVATION_CODE_VALID;
        activationCodePage.showError = true;
        assertTrue(input.invalid);

        // Should be showing the start scanning UI.
        assertFalse(startScanningContainer.hidden);
        assertTrue(scanFinishContainer.hidden);
      });

  test(
      'Install error after scanning should show error on camera',
      async function() {
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
        const getUseCameraAgainButton = () => {
          return activationCodePage.$$('#useCameraAgainButton');
        };
        assertTrue(!!input);
        assertTrue(!!startScanningContainer);
        assertTrue(!!startScanningButton);
        assertTrue(!!scanFinishContainer);
        assertTrue(!!scanInstallFailureHeader);
        assertTrue(!!scanSucessHeader);
        assertFalse(!!getUseCameraAgainButton());
        assertFalse(input.invalid);

        // Click the start scanning button.
        startScanningButton.click();
        mediaDevices.resolveGetUserMedia();
        await waitAfterNextRender(activationCodePage);

        // Mock camera scanning a code.
        await intervalFunction();
        await flushAsync();

        // The code detected UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertFalse(scanSucessHeader.hidden);
        assertTrue(scanInstallFailureHeader.hidden);
        assertFalse(!!getUseCameraAgainButton());

        // Mock an install error.
        activationCodePage.showError = true;
        await flushAsync();

        // The scan install failure UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertTrue(scanSucessHeader.hidden);
        assertFalse(scanInstallFailureHeader.hidden);
        assertTrue(!!getUseCameraAgainButton());

        // There should be no error displayed on the input.
        assertFalse(input.invalid);
      });

  test('Tabbing does not close video stream', async function() {
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const getVideo = () => activationCodePage.$$('#video');
    const input = activationCodePage.$$('#activationCode');

    assertTrue(!!startScanningButton);
    assertTrue(!!getVideo());
    assertTrue(getVideo().hidden);
    assertTrue(!!input);

    // Click the start scanning button.
    startScanningButton.click();
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);

    assertFalse(getVideo().hidden);

    // Simulate keyboard 'Tab' key press.
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Tab'}));
    await flushAsync();

    assertFalse(getVideo().hidden);

    // Simulate keyboard 'A' key press.
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'KeyA'}));
    await flushAsync();

    assertTrue(getVideo().hidden);
  });

  test(
      'Clear qr code detection timeout when video is hidden', async function() {
        const startScanningButton =
            activationCodePage.$$('#startScanningButton');
        const getVideo = () => activationCodePage.$$('#video');

        assertTrue(!!startScanningButton);
        assertTrue(!!getVideo());
        assertTrue(getVideo().hidden);

        // Click the start scanning button.
        startScanningButton.click();
        mediaDevices.resolveGetUserMedia();
        await waitAfterNextRender(activationCodePage);

        assertFalse(getVideo().hidden);
        assertTrue(!!activationCodePage.getQrCodeDetectorTimerForTest());

        // Mock camera scanning a code.
        await intervalFunction();
        await flushAsync();

        assertFalse(!!activationCodePage.getQrCodeDetectorTimerForTest());
      });

  test('Input entered manually is validated', async function() {
    const input = activationCodePage.$$('#activationCode');
    assertTrue(!!input);
    assertFalse(input.invalid);

    const setInputAndAssert =
        async (activationCode, isInputInvalid, shouldEventContainCode) => {
      const activationCodeUpdatedPromise =
          eventToPromise('activation-code-updated', activationCodePage);
      input.value = activationCode;
      const activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
      assertFalse(activationCodePage.isFromQrCode);
      assertEquals(
          activationCodeUpdatedEvent.detail.activationCode,
          shouldEventContainCode ? activationCode : null);
      assertEquals(input.invalid, isInputInvalid);
      assertEquals(activationCodePage.$.inputSubtitle.hidden, isInputInvalid);
      assertEquals(
          activationCodePage.$.inputSubtitle.innerText.trim(),
          loadTimeData.getString('scanQrCodeInputSubtitle'));
    };

    await setInputAndAssert(
        /*activationCode=*/ 'U', /*isInputValid=*/ true,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'L', /*isInputInvalid=*/ false,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'Lp', /*isInputInvalid=*/ true,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'LP', /*isInputInvalid=*/ false,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'LPA:1#', /*isInputInvalid=*/ true,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'LPA:1$', /*isInputInvalid=*/ false,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'LPA:1#ACTIVATION_CODE', /*isInputInvalid=*/ true,
        /*shouldEventContainCode=*/ false);
    await setInputAndAssert(
        /*activationCode=*/ 'LPA:1$ACTIVATION_CODE', /*isInputInvalid=*/ false,
        /*shouldEventContainCode=*/ true);

    // Erase the code so that it's incomplete. The event should no longer
    // contain the code.
    await setInputAndAssert(
        /*activationCode=*/ 'LPA:1$', /*isInputInvalid=*/ false,
        /*doesEventContainCode=*/ false);
  });

  test('Scanned code is validated', async function() {
    const input = activationCodePage.$$('#activationCode');
    const startScanningContainer =
        activationCodePage.$$('#startScanningContainer');
    const startScanningButton = activationCodePage.$$('#startScanningButton');
    const scanFinishContainer = activationCodePage.$$('#scanFinishContainer');
    const scanInstallFailureHeader =
        activationCodePage.$$('#scanInstallFailureHeader');
    const scanSucessHeader = activationCodePage.$$('#scanSucessHeader');
    const getUseCameraAgainButton = () => {
      return activationCodePage.$$('#useCameraAgainButton');
    };
    assertTrue(!!input);
    assertTrue(!!startScanningContainer);
    assertTrue(!!startScanningButton);
    assertTrue(!!scanFinishContainer);
    assertTrue(!!scanInstallFailureHeader);
    assertTrue(!!scanSucessHeader);
    assertFalse(!!getUseCameraAgainButton());
    assertFalse(input.invalid);

    // Click the start scanning button.
    startScanningButton.click();
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);

    // Mock camera scanning an invalid code.
    let activationCodeUpdatedPromise =
        eventToPromise('activation-code-updated', activationCodePage);
    FakeBarcodeDetector.setDetectedBarcode(ACTIVATION_CODE_INVALID);
    await intervalFunction();
    await flushAsync();

    // The scan install failure UI should be showing.
    assertTrue(startScanningContainer.hidden);
    assertFalse(scanFinishContainer.hidden);
    assertTrue(scanSucessHeader.hidden);
    assertFalse(scanInstallFailureHeader.hidden);
    assertTrue(!!getUseCameraAgainButton());
    assertTrue(input.invalid);
    let activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertFalse(!!activationCodeUpdatedEvent.detail.activationCode);

    // Start scanning again.
    getUseCameraAgainButton().click();
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);

    // Mock camera scanning a valid, incomplete code.
    activationCodeUpdatedPromise =
        eventToPromise('activation-code-updated', activationCodePage);
    FakeBarcodeDetector.setDetectedBarcode(/*barcode=*/ 'LPA:');
    await intervalFunction();
    await flushAsync();

    // The scan install failure UI should be showing.
    assertTrue(startScanningContainer.hidden);
    assertFalse(scanFinishContainer.hidden);
    assertTrue(scanSucessHeader.hidden);
    assertFalse(scanInstallFailureHeader.hidden);
    assertTrue(!!getUseCameraAgainButton());
    assertFalse(input.invalid);
    activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertFalse(!!activationCodeUpdatedEvent.detail.activationCode);

    // Start scanning again.
    getUseCameraAgainButton().click();
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);

    // Mock camera scanning a valid code.
    activationCodeUpdatedPromise =
        eventToPromise('activation-code-updated', activationCodePage);
    FakeBarcodeDetector.setDetectedBarcode(ACTIVATION_CODE_VALID);
    await intervalFunction();
    await flushAsync();

    // The code detected UI should be showing.
    assertTrue(activationCodePage.isFromQrCode);
    assertTrue(startScanningContainer.hidden);
    assertFalse(scanFinishContainer.hidden);
    assertFalse(scanSucessHeader.hidden);
    assertTrue(scanInstallFailureHeader.hidden);
    assertFalse(!!getUseCameraAgainButton());
    assertFalse(input.invalid);
    activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertEquals(
        activationCodeUpdatedEvent.detail.activationCode,
        ACTIVATION_CODE_VALID);
  });
});
