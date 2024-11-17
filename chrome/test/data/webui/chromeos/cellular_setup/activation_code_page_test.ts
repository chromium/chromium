// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/activation_code_page.js';

import type {ActivationCodePageElement} from 'chrome://resources/ash/common/cellular_setup/activation_code_page.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.js';
import {FakeMediaDevices} from './fake_media_devices.js';

suite('CrComponentsActivationCodePageTest', function() {
  const ACTIVATION_CODE_VALID = 'LPA:1$ACTIVATION_CODE';
  const ACTIVATION_CODE_INVALID = 'INVALID';

  let activationCodePage: ActivationCodePageElement;
  let mediaDevices: FakeMediaDevices|null = null;
  let intervalFunction: Function|null = null;
  let networkConfigRemote: FakeNetworkConfig;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  // Captures the function that is called every time the interval timer
  // timeouts.
  function setIntervalFunction(fn: Function, _: number): number {
    intervalFunction = fn;
    return 1;
  }

  // In tests, pausing the video can have race conditions with previous
  // requests to play the video due to the speed of execution. Avoid this by
  // mocking the play and pause actions.
  function playVideoFunction(): void {}

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigRemote);
    networkConfigRemote.setDeviceStateForTest({
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: undefined,
      scanning: false,
      simLockStatus: undefined,
      simInfos: undefined,
      inhibitReason: InhibitReason.kNotInhibited,
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: true,
      isFlashing: false,
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
    });

    await flushAsync();

    activationCodePage = document.createElement('activation-code-page');
    await activationCodePage.setFakesForTesting(
        FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
        playVideoFunction);
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
    assertTrue(!!mediaDevices);
    mediaDevices.addDevice();
    await flushAsync();

    await resolveEnumeratedDevicesPromise();
  }

  async function resolveEnumeratedDevicesPromise() {
    let resolver: () => void;
    const enumerateDeviceResolvedPromise = new Promise<void>((resolve, _) => {
      resolver = resolve;
    });
    assertTrue(!!mediaDevices);
    mediaDevices.resolveEnumerateDevices(function() {
      resolver();
    });
    await enumerateDeviceResolvedPromise;
  }

  test('Page description', async function() {
    const description =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#description');
    assertTrue(!!description);

    // Mock camera on
    activationCodePage.showNoProfilesFound = true;
    assertEquals(
        description.innerText!.trim(),
        loadTimeData.getString('scanQRCodeNoProfilesFound'));
    activationCodePage.showNoProfilesFound = false;
    assertEquals(
        description.innerText!.trim(), loadTimeData.getString('scanQRCode'));

    // Clearing devices to test without camera
    assertTrue(!!mediaDevices);
    mediaDevices.removeDevice();
      await resolveEnumeratedDevicesPromise();
    activationCodePage.showNoProfilesFound = true;
    assertEquals(
        description.innerText!.trim(),
        loadTimeData.getString('enterActivationCodeNoProfilesFound'));
    activationCodePage.showNoProfilesFound = false;
    assertEquals(
        description.innerText!.trim(),
        loadTimeData.getString('enterActivationCode'));
  });

  test('UI states', async function() {
    let qrCodeDetectorContainer =
        activationCodePage.shadowRoot!.querySelector('#esimQrCodeDetection');
    const activationCodeContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#activationCodeContainer');
    const video =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>('#video');
    const startScanningContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningContainer');
    const startScanningButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningButton');
    const scanFinishContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanFinishContainer');
    const switchCameraButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#switchCameraButton');
    const getUseCameraAgainButton = () => {
      return activationCodePage.shadowRoot!.querySelector<HTMLElement>(
          '#useCameraAgainButton');
    };
    const scanSuccessContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanSuccessContainer');
    const scanFailureContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanFailureContainer');

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

    // Initial state should only be showing the start scanning UI.
    assertFalse(startScanningContainer.hidden);
    assertFalse(activationCodeContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    // Click the start scanning button.
    startScanningButton.click();
    assertTrue(!!mediaDevices);
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
    assertTrue(!!intervalFunction);
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
    const input = activationCodePage.shadowRoot!.querySelector<HTMLElement>(
        '#activationCode');
    assertTrue(!!input);
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'A'}));
    await flushAsync();

    // We should be back in the initial state.
    assertFalse(startScanningContainer.hidden);
    assertFalse(activationCodeContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(scanFinishContainer.hidden);
    assertTrue(switchCameraButton.hidden);

    await flushAsync();

    // Mock, no media devices present
    mediaDevices.removeDevice();
    await resolveEnumeratedDevicesPromise();

    // When no camera device is present qrCodeDetector container should
    // not be shown
    qrCodeDetectorContainer =
        activationCodePage.shadowRoot!.querySelector('#esimQrCodeDetection');

    assertFalse(!!qrCodeDetectorContainer);
  });

  test('Switch camera button states', async function() {
    const video =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>('#video');
    const startScanningButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningButton');
    const switchCameraButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#switchCameraButton');

    assertTrue(!!video);
    assertTrue(!!startScanningButton);
    assertTrue(!!switchCameraButton);

    // Initial state should only be showing the start scanning UI.
    assertTrue(video.hidden);
    assertTrue(switchCameraButton.hidden);

    // Click the start scanning button.
    startScanningButton.click();
    assertTrue(!!mediaDevices);
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
    assertTrue(!!intervalFunction);
    await intervalFunction();
    await flushAsync();

    assertTrue(video.hidden);
  });

  test('Opening multiple streams is not supported', async function() {
    assertTrue(!!mediaDevices);
    mediaDevices.setShouldUserMediaRequestFail(true);

    const video =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>('#video');
    const startScanningButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningButton');
    const switchCameraButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#switchCameraButton');
    const scanFailureContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanFailureContainer');

    // Confirm the UI starts in a good state.
    assertTrue(!!video);
    assertTrue(!!startScanningButton);
    assertTrue(!!switchCameraButton);
    assertTrue(!!scanFailureContainer);

    // Initial state should only be showing the start scanning UI.
    assertTrue(video.hidden);
    assertTrue(switchCameraButton.hidden);
    assertTrue(scanFailureContainer.hidden);

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

    // The failure message should be visible as multiple media streams are not
    // allowed.
    assertFalse(scanFailureContainer.hidden);
    assertTrue(video.hidden);
    assertTrue(switchCameraButton.hidden);
  });

  test(
      'Do not show qrContainer when BarcodeDetector is not ready',
      async function() {
        let qrCodeDetectorContainer =
            activationCodePage.shadowRoot!.querySelector(
                '#esimQrCodeDetection');

        assertTrue(!!qrCodeDetectorContainer);
        // Activation code input should be at the bottom of the page.
        const activationCodeContainer =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#activationCodeContainer');
        assertTrue(!!activationCodeContainer);
        assertTrue(activationCodeContainer.classList.contains('relative'));

        FakeBarcodeDetector.setShouldFail(true);
        await activationCodePage.setFakesForTesting(
            FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
            playVideoFunction);

        qrCodeDetectorContainer = activationCodePage.shadowRoot!.querySelector(
            '#esimQrCodeDetection');

        assertFalse(!!qrCodeDetectorContainer);
        // Activation code input should now be in the center of the page.
        assertTrue(!!activationCodeContainer);
        assertTrue(activationCodeContainer.classList.contains('center'));
      });

  test('Event is fired when enter is pressed on input', async function() {
    let eventFired = false;
    activationCodePage.addEventListener('forward-navigation-requested', () => {
      eventFired = true;
    });
    const input = activationCodePage.shadowRoot!.querySelector<HTMLElement>(
        '#activationCode');
    assertTrue(!!input);
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await flushAsync();
    assertTrue(eventFired);
  });

  test(
      'Install error after manual entry should show error on input',
      async function() {
        const input =
            activationCodePage.shadowRoot!.querySelector<CrInputElement>(
                '#activationCode');
        const startScanningContainer =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#startScanningContainer');
        const scanFinishContainer =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#scanFinishContainer');
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
        const input =
            activationCodePage.shadowRoot!.querySelector<CrInputElement>(
                '#activationCode');
        const startScanningContainer =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#startScanningContainer');
        const startScanningButton =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#startScanningButton');
        const scanFinishContainer =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#scanFinishContainer');
        const scanInstallFailureHeader =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#scanInstallFailureHeader');
        const scanSuccessHeader =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#scanSuccessHeader');
        const getUseCameraAgainButton = () => {
          return activationCodePage.shadowRoot!.querySelector<HTMLElement>(
              '#useCameraAgainButton');
        };
        assertTrue(!!input);
        assertTrue(!!startScanningContainer);
        assertTrue(!!startScanningButton);
        assertTrue(!!scanFinishContainer);
        assertTrue(!!scanInstallFailureHeader);
        assertTrue(!!scanSuccessHeader);
        assertFalse(!!getUseCameraAgainButton());
        assertFalse(input.invalid);

        // Click the start scanning button.
        startScanningButton.click();
        assertTrue(!!mediaDevices);
        mediaDevices.resolveGetUserMedia();
        await waitAfterNextRender(activationCodePage);

        // Mock camera scanning a code.
        assertTrue(!!intervalFunction);
        await intervalFunction();
        await flushAsync();

        // The code detected UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertFalse(scanSuccessHeader.hidden);
        assertTrue(scanInstallFailureHeader.hidden);
        assertFalse(!!getUseCameraAgainButton());

        // Mock an install error.
        activationCodePage.showError = true;
        await flushAsync();

        // The scan install failure UI should be showing.
        assertTrue(startScanningContainer.hidden);
        assertFalse(scanFinishContainer.hidden);
        assertTrue(scanSuccessHeader.hidden);
        assertFalse(scanInstallFailureHeader.hidden);
        assertTrue(!!getUseCameraAgainButton());

        // There should be no error displayed on the input.
        assertFalse(input.invalid);
      });

  test('Tabbing does not close video stream', async function() {
    const startScanningButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningButton');
    const getVideo = () =>
        activationCodePage.shadowRoot!.querySelector<HTMLElement>('#video');
    const input = activationCodePage.shadowRoot!.querySelector<HTMLElement>(
        '#activationCode');

    assertTrue(!!startScanningButton);
    let video = getVideo();
    assertTrue(!!video);
    assertTrue(video.hidden);
    assertTrue(!!input);

    // Click the start scanning button.
    startScanningButton.click();
    assertTrue(!!mediaDevices);
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);
    video = getVideo();
    assertTrue(!!video);
    assertFalse(video.hidden);

    // Simulate keyboard 'Tab' key press.
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Tab'}));
    await flushAsync();

    video = getVideo();
    assertTrue(!!video);
    assertFalse(video.hidden);

    // Simulate keyboard 'A' key press.
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'KeyA'}));
    await flushAsync();

    video = getVideo();
    assertTrue(!!video);
    assertTrue(video.hidden);
  });

  test(
      'Clear qr code detection timeout when video is hidden', async function() {
        const startScanningButton =
            activationCodePage.shadowRoot!.querySelector<HTMLElement>(
                '#startScanningButton');
        const getVideo = () =>
            activationCodePage.shadowRoot!.querySelector<HTMLElement>('#video');

        assertTrue(!!startScanningButton);
        let video = getVideo();
        assertTrue(!!video);
        assertTrue(video.hidden);

        // Click the start scanning button.
        startScanningButton.click();
        assertTrue(!!mediaDevices);
        mediaDevices.resolveGetUserMedia();
        await waitAfterNextRender(activationCodePage);

        video = getVideo();
        assertTrue(!!video);
        assertFalse(video.hidden);
        assertTrue(!!activationCodePage.getQrCodeDetectorTimerForTest());

        // Mock camera scanning a code.
        assertTrue(!!intervalFunction);
        await intervalFunction();
        await flushAsync();

        assertFalse(!!activationCodePage.getQrCodeDetectorTimerForTest());
      });

  test('Input entered manually is validated', async function() {
    const input = activationCodePage.shadowRoot!.querySelector<CrInputElement>(
        '#activationCode');
    assertTrue(!!input);
    assertFalse(input.invalid);

    const setInputAndAssert = async (
        activationCode: string, isInputInvalid: boolean,
        shouldEventContainCode: boolean) => {
      const activationCodeUpdatedPromise =
          eventToPromise('activation-code-updated', activationCodePage);
      input.value = activationCode;
      const activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
      assertFalse(activationCodePage.isFromQrCode);
      assertEquals(
          activationCodeUpdatedEvent.detail.activationCode,
          shouldEventContainCode ? activationCode : null);
      assertEquals(input.invalid, isInputInvalid);
      const inputSubtitle =
          activationCodePage.shadowRoot!.querySelector<HTMLElement>(
              '#inputSubtitle');
      assertTrue(!!inputSubtitle);
      assertEquals(inputSubtitle.hidden, isInputInvalid);
      assertEquals(
          inputSubtitle.innerText!.trim(),
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
    const input = activationCodePage.shadowRoot!.querySelector<CrInputElement>(
        '#activationCode');
    const startScanningContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningContainer');
    const startScanningButton =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#startScanningButton');
    const scanFinishContainer =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanFinishContainer');
    const scanInstallFailureHeader =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanInstallFailureHeader');
    const scanSuccessHeader =
        activationCodePage.shadowRoot!.querySelector<HTMLElement>(
            '#scanSuccessHeader');
    const getUseCameraAgainButton = () => {
      return activationCodePage.shadowRoot!.querySelector<HTMLElement>(
          '#useCameraAgainButton');
    };
    assertTrue(!!input);
    assertTrue(!!startScanningContainer);
    assertTrue(!!startScanningButton);
    assertTrue(!!scanFinishContainer);
    assertTrue(!!scanInstallFailureHeader);
    assertTrue(!!scanSuccessHeader);
    assertFalse(!!getUseCameraAgainButton());
    assertFalse(input.invalid);

    // Click the start scanning button.
    startScanningButton.click();
    assertTrue(!!mediaDevices);
    mediaDevices.resolveGetUserMedia();
    await waitAfterNextRender(activationCodePage);

    // Mock camera scanning an invalid code.
    let activationCodeUpdatedPromise =
        eventToPromise('activation-code-updated', activationCodePage);
    FakeBarcodeDetector.setDetectedBarcode(ACTIVATION_CODE_INVALID);
    assertTrue(!!intervalFunction);
    await intervalFunction();
    await flushAsync();

    // The scan install failure UI should be showing.
    assertTrue(startScanningContainer.hidden);
    assertFalse(scanFinishContainer.hidden);
    assertTrue(scanSuccessHeader.hidden);
    assertFalse(scanInstallFailureHeader.hidden);
    assertTrue(!!getUseCameraAgainButton());
    assertTrue(input.invalid);
    let activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertFalse(!!activationCodeUpdatedEvent.detail.activationCode);

    // Start scanning again.
    getUseCameraAgainButton()!.click();
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
    assertTrue(scanSuccessHeader.hidden);
    assertFalse(scanInstallFailureHeader.hidden);
    assertTrue(!!getUseCameraAgainButton());
    assertFalse(input.invalid);
    activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertFalse(!!activationCodeUpdatedEvent.detail.activationCode);

    // Start scanning again.
    getUseCameraAgainButton()!.click();
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
    assertFalse(scanSuccessHeader.hidden);
    assertTrue(scanInstallFailureHeader.hidden);
    assertFalse(!!getUseCameraAgainButton());
    assertFalse(input.invalid);
    activationCodeUpdatedEvent = await activationCodeUpdatedPromise;
    assertEquals(
        activationCodeUpdatedEvent.detail.activationCode,
        ACTIVATION_CODE_VALID);
  });

  test('check carrier lock warning', async function() {
    assertTrue(!!activationCodePage.shadowRoot!.querySelector(
        '#carrierLockWarningContainer'));
  });

  test(
      'check carrier lock warning not displayed for consumer devices',
      async function() {
        networkConfigRemote.setDeviceStateForTest({
          ipv4Address: undefined,
          ipv6Address: undefined,
          imei: undefined,
          macAddress: undefined,
          scanning: false,
          simLockStatus: undefined,
          simInfos: undefined,
          inhibitReason: InhibitReason.kNotInhibited,
          simAbsent: false,
          managedNetworkAvailable: false,
          serial: undefined,
          isCarrierLocked: false,
          isFlashing: false,
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
        });
        await flushAsync();
        const page = document.createElement('activation-code-page');
        assertFalse(
            !!page.shadowRoot?.querySelector('#carrierLockWarningContainer'));
      });
});
