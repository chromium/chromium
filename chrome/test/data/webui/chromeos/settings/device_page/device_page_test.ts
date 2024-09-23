// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrIconButtonElement, crosAudioConfigMojom, CrSliderElement, CrToggleElement, DevicePageBrowserProxyImpl, fakeCrosAudioConfig, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads, Route, Router, routes, setCrosAudioConfigForTesting, setDisplayApiForTesting, setInputDeviceSettingsProviderForTesting, SettingsAudioElement, SettingsDevicePageElement, SettingsPerDeviceKeyboardElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';
import {clearBody} from '../utils.js';

import {getFakePrefs, pressArrowLeft, pressArrowRight, simulateSliderClicked} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-device-page>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let devicePage: SettingsDevicePageElement;
  let fakeSystemDisplay: FakeSystemDisplay;
  let browserProxy: TestDevicePageBrowserProxy;

  function showAndGetDeviceSubpage(
      subpage: string, expectedRoute: Route): HTMLElement {
    const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
        `#main #${subpage}Row`);
    assertTrue(!!row);
    row.click();
    assertEquals(expectedRoute, Router.getInstance().currentRoute);
    const page = devicePage.shadowRoot!.querySelector<HTMLElement>(
        'settings-' + subpage);
    assertTrue(!!page);
    return page;
  }

  setup(async () => {
    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay);

    Router.getInstance().navigateTo(routes.DEVICE);

    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    setDeviceSplitEnabled(true);
  });

  async function init(): Promise<void> {
    clearBody();
    devicePage = document.createElement('settings-device-page');
    devicePage.prefs = getFakePrefs();
    document.body.appendChild(devicePage);
    await flushTasks();
  }

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Set enableInputDeviceSettingsSplit feature flag to true for split tests.
   */
  function setDeviceSplitEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: isEnabled,
    });
  }

  /**
   * Set enablePeripheralCustomization feature flag to true for split tests.
   */
  function setPeripheralCustomizationEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enablePeripheralCustomization: isEnabled,
    });
  }

  /**
   * Set enableAudioHfpMicSRToggle feature flag to true for tests.
   */
  function setEnableAudioHfpMicSRToggleEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableAudioHfpMicSRToggle: isEnabled,
    });
  }


  test('device page', async () => {
    const provider = new FakeInputDeviceSettingsProvider();
    setInputDeviceSettingsProviderForTesting(provider);
    provider.setFakeMice(fakeMice);
    provider.setFakePointingSticks(fakePointingSticks);
    provider.setFakeTouchpads(fakeTouchpads);
    provider.setFakeGraphicsTablets(fakeGraphicsTablets);

    await init();
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#displayRow')));
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#audioRow')));

    // enableInputDeviceSettingsSplit feature flag by default is turned on.
    assertTrue(
        isVisible(devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot!.querySelector('#perDevicePointingStickRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot!.querySelector('#perDeviceKeyboardRow')));
    assertFalse(
        isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));
    assertFalse(
        isVisible(devicePage.shadowRoot!.querySelector('#keyboardRow')));

    // enablePeripheralCustomization feature flag by default is turned on.
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#tabletRow')));

    // Turn off the enableInputDeviceSettingsSplit feature flag.
    setDeviceSplitEnabled(false);
    await init();
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#keyboardRow')));

    webUIListenerCallback('has-mouse-changed', false);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));

    webUIListenerCallback('has-pointing-stick-changed', false);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));

    webUIListenerCallback('has-touchpad-changed', false);
    await flushTasks();
    assertFalse(
        isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));

    webUIListenerCallback('has-mouse-changed', true);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot!.querySelector('#pointersRow')));

    // Turn off the enablePeripheralCustomization feature flag.
    setPeripheralCustomizationEnabled(false);
    await init();
    assertFalse(isVisible(devicePage.shadowRoot!.querySelector('#tabletRow')));
  });

  test('per-device-mouse row visibility', async () => {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(
        isVisible(devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));
  });

  test(
      'per-device-mouse row visibility based on devices connected',
      async () => {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakeMice(fakeMice);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));

        provider.setFakeMice([]);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));

        provider.setFakeMice(fakeMice);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-mouse-changed', true);
        await init();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));

        webUIListenerCallback('has-mouse-changed', false);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceMouseRow')));
      });

  test('per-device-touchpad row visibility', async () => {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));
  });

  test(
      'per-device-touchpad row visiblity based on connected devices',
      async () => {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakeTouchpads(fakeTouchpads);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));

        provider.setFakeTouchpads([]);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));

        provider.setFakeTouchpads(fakeTouchpads);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-touchpad-changed', true);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));

        webUIListenerCallback('has-touchpad-changed', false);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot!.querySelector('#perDeviceTouchpadRow')));
      });

  test('per-device-pointing-stick row visibility', async () => {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot!.querySelector('#perDevicePointingStickRow')));
  });

  test(
      'per-device-pointing-stick row visibility based on devices connected',
      async () => {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakePointingSticks(fakePointingSticks);
        await flushTasks();
        assertTrue(isVisible(devicePage.shadowRoot!.querySelector(
            '#perDevicePointingStickRow')));

        provider.setFakePointingSticks([]);
        await flushTasks();
        assertFalse(isVisible(devicePage.shadowRoot!.querySelector(
            '#perDevicePointingStickRow')));

        provider.setFakePointingSticks(fakePointingSticks);
        await flushTasks();
        assertTrue(isVisible(devicePage.shadowRoot!.querySelector(
            '#perDevicePointingStickRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-pointing-stick-stick-changed', true);
        await flushTasks();
        assertFalse(isVisible(devicePage.shadowRoot!.querySelector(
            '#perDevicePointingStickRow')));

        webUIListenerCallback('has-pointing-stick-changed', false);
        await flushTasks();
        assertFalse(isVisible(devicePage.shadowRoot!.querySelector(
            '#perDevicePointingStickRow')));
      });

  test('per-device-keyboard row visibility', async () => {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot!.querySelector('#perDeviceKeyboardRow')));
  });

  suite('graphics tablet subpage', () => {
    function queryTabletRow(): HTMLElement|null {
      return devicePage.shadowRoot!.querySelector<HTMLElement>('#tabletRow');
    }
    test(
        'graphics tablet row visibility depends on devices connected',
        async () => {
          const provider = new FakeInputDeviceSettingsProvider();
          setInputDeviceSettingsProviderForTesting(provider);
          provider.setFakeGraphicsTablets(fakeGraphicsTablets);

          // Tests with flag on.
          setPeripheralCustomizationEnabled(true);
          await init();

          assertTrue(isVisible(queryTabletRow()));

          provider.setFakeGraphicsTablets([]);
          await flushTasks();
          assertFalse(isVisible(queryTabletRow()));

          provider.setFakeGraphicsTablets(fakeGraphicsTablets);
          await flushTasks();
          assertTrue(isVisible(queryTabletRow()));
        });

    test('graphics tablet subpage navigates back to device page', async () => {
      const provider = new FakeInputDeviceSettingsProvider();
      setInputDeviceSettingsProviderForTesting(provider);
      provider.setFakeGraphicsTablets(fakeGraphicsTablets);

      // Tests with flag on.
      setPeripheralCustomizationEnabled(true);
      await init();

      const row = queryTabletRow();
      assertTrue(!!row);
      row.click();
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);

      provider.setFakeGraphicsTablets([]);
      await flushTasks();
      assertEquals(routes.DEVICE, Router.getInstance().currentRoute);
    });
  });

  suite('audio', () => {
    let audioPage: SettingsAudioElement;
    let crosAudioConfig: fakeCrosAudioConfig.FakeCrosAudioConfig;

    // Static test audio system properties.
    const maxVolumePercentFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 100,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],
      inputDevices: [],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const minVolumePercentFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],
      inputDevices: [],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const mutedByUserFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
    };

    const mutedByPolicyFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByPolicy,
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByPolicy,
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
    };

    const mutedExternallyFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
    };

    const emptyOutputDevicesFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const emptyInputDevicesFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [
        fakeCrosAudioConfig.fakeSpeakerActive,
        fakeCrosAudioConfig.fakeMicJackInactive,
      ],
      inputDevices: [],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const activeSpeakerFakeAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 75,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [
        fakeCrosAudioConfig.fakeSpeakerActive,
        fakeCrosAudioConfig.fakeMicJackInactive,
      ],
      inputDevices: [],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const noiseCancellationNotSupportedAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const styleTransferSupportedAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActiveWithStyleTransfer,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const styleTransferNotSupportedAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };


    const hfpMicSrNotSupportedAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeBluetoothMic,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const hfpMicSrSupportedAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeBluetoothNbMicActiveHfpMicSrNotEnabled,
        fakeCrosAudioConfig.fakeMicJackInactive,
      ],
      inputGainPercent: 0,
      inputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };

    const muteByHardwareAudioSystemProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
      outputDevices: [],
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,
      inputGainPercent: 0,
    };

    setup(async () => {
      await init();

      // FakeAudioConfig must be set before audio subpage is loaded.
      crosAudioConfig = new fakeCrosAudioConfig.FakeCrosAudioConfig();
      setCrosAudioConfigForTesting(crosAudioConfig);
      // Ensure data reset to fresh state.
      crosAudioConfig.setAudioSystemProperties(
          {...fakeCrosAudioConfig.defaultFakeAudioSystemProperties});
      const page = showAndGetDeviceSubpage('audio', routes.AUDIO) as
          SettingsAudioElement;
      audioPage = page;
      await flushTasks();
    });

    teardown(() => {
      audioPage.remove();
    });

    test('subpage visibility', () => {
      assertEquals(routes.AUDIO, Router.getInstance().currentRoute);
      assertTrue(
          isVisible(audioPage.shadowRoot!.querySelector('#audioOutputTitle')));
      assertTrue(isVisible(
          audioPage.shadowRoot!.querySelector('#audioOutputSubsection')));
      assertTrue(
          isVisible(audioPage.shadowRoot!.querySelector('#audioInputSection')));
      const inputSectionHeader =
          audioPage.shadowRoot!.querySelector('#audioInputTitle');
      assertTrue(!!inputSectionHeader);
      assertEquals('Input', inputSectionHeader.textContent!.trim());
      const inputDeviceSubsectionHeader =
          audioPage.shadowRoot!.querySelector('#audioInputDeviceLabel');
      assertTrue(!!inputDeviceSubsectionHeader);
      assertEquals(
          'Microphone', inputDeviceSubsectionHeader.textContent!.trim());
      const inputDeviceSubsectionDropdown =
          audioPage.shadowRoot!.querySelector('#audioInputDeviceDropdown');
      assertTrue(isVisible(inputDeviceSubsectionDropdown));
      const inputGainSubsectionHeader =
          audioPage.shadowRoot!.querySelector('#audioInputGainLabel');
      assertTrue(!!inputGainSubsectionHeader, 'audioInputGainLabel');
      assertEquals('Volume', inputGainSubsectionHeader.textContent!.trim());
      const inputVolumeButton =
          audioPage.shadowRoot!.querySelector('#audioInputGainMuteButton');
      assertTrue(isVisible(inputVolumeButton), 'audioInputGainMuteButton');
      const inputVolumeSlider =
          audioPage.shadowRoot!.querySelector('#audioInputGainVolumeSlider');
      assertTrue(isVisible(inputVolumeSlider), 'audioInputGainVolumeSlider');
      const noiseCancellationSubsectionHeader =
          audioPage.shadowRoot!.querySelector(
              '#audioInputNoiseCancellationLabel');
      assertTrue(!!noiseCancellationSubsectionHeader);
      assertEquals(
          'Noise cancellation',
          noiseCancellationSubsectionHeader.textContent!.trim());
      const noiseCancellationToggle = audioPage.shadowRoot!.querySelector(
          '#audioInputNoiseCancellationToggle');
      assertTrue(isVisible(noiseCancellationToggle));
    });

    test('output volume mojo test', async () => {
      const outputVolumeSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(
              '#outputVolumeSlider');
      assertTrue(!!outputVolumeSlider);
      // Test default properties.
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties
              .outputVolumePercent,
          outputVolumeSlider.value);

      // Test min volume case.
      crosAudioConfig.setAudioSystemProperties(
          minVolumePercentFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          minVolumePercentFakeAudioSystemProperties.outputVolumePercent,
          outputVolumeSlider.value);

      // Test max volume case.
      crosAudioConfig.setAudioSystemProperties(
          maxVolumePercentFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          maxVolumePercentFakeAudioSystemProperties.outputVolumePercent,
          outputVolumeSlider.value);
    });

    test('simulate setting output volume slider mojo test', async () => {
      const sliderSelector = '#outputVolumeSlider';
      const outputSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(sliderSelector);
      assertTrue(!!outputSlider);
      const outputMuteButton =
          audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
              '#audioOutputMuteButton');
      assertTrue(!!outputMuteButton);

      // Test clicking to min volume case.
      const minOutputVolumePercent = 0;
      await simulateSliderClicked(outputSlider, minOutputVolumePercent);
      assertEquals(
          minOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent,
      );
      assertEquals('settings20:volume-zero', outputMuteButton.ironIcon);

      // Test clicking to max volume case.
      const maxOutputVolumePercent = 100;
      await simulateSliderClicked(outputSlider, maxOutputVolumePercent);
      assertEquals(
          maxOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent,
      );
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Test clicking to non-boundary volume case.
      const nonBoundaryOutputVolumePercent = 50;
      await simulateSliderClicked(outputSlider, nonBoundaryOutputVolumePercent);
      assertEquals(
          nonBoundaryOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent,
      );
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Ensure value clamps to min.
      outputSlider.value = -1;
      outputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          minOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent);
      assertEquals('settings20:volume-zero', outputMuteButton.ironIcon);

      // Ensure value clamps to max.
      outputSlider.value = 101;
      outputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          maxOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent);
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Test clicking to a small icon volume case.
      const smallIconOutputVolumePercent = 10;
      await simulateSliderClicked(outputSlider, smallIconOutputVolumePercent);
      assertEquals(
          smallIconOutputVolumePercent,
          audioPage.get('audioSystemProperties_').outputVolumePercent,
      );
      assertEquals('settings20:volume-down', outputMuteButton.ironIcon);
    });

    test('output mute state changes slider disabled state', async () => {
      const outputVolumeSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(
              '#outputVolumeSlider');
      assertTrue(!!outputVolumeSlider);

      // Test default properties.
      assertFalse(audioPage.getIsOutputMutedForTest());
      assertFalse(outputVolumeSlider.disabled);

      // Test muted by user case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByUserFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(audioPage.getIsOutputMutedForTest());
      assertFalse(outputVolumeSlider.disabled);

      // Test muted by policy case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(audioPage.getIsOutputMutedForTest());
      assertTrue(outputVolumeSlider.disabled);
    });

    test('output device mojo test', async () => {
      const outputDeviceDropdown =
          audioPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#audioOutputDeviceDropdown');
      assertTrue(!!outputDeviceDropdown);

      // Test default properties.
      assertEquals(
          fakeCrosAudioConfig.defaultFakeMicJack.id,
          BigInt(outputDeviceDropdown.value));
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties.outputDevices
              .length,
          outputDeviceDropdown.length);

      // Test empty output devices case.
      crosAudioConfig.setAudioSystemProperties(
          emptyOutputDevicesFakeAudioSystemProperties);
      await flushTasks();
      assertEquals('', outputDeviceDropdown.value);
      assertEquals(
          emptyOutputDevicesFakeAudioSystemProperties.outputDevices.length,
          outputDeviceDropdown.length);

      // If the output devices are empty, the output section should be hidden.
      const outputDeviceSection =
          audioPage.shadowRoot!.querySelector('#output');
      assertFalse(isVisible(outputDeviceSection));
      const inputDeviceSection = audioPage.shadowRoot!.querySelector('#input');
      assertTrue(isVisible(inputDeviceSection));

      // Test active speaker case.
      crosAudioConfig.setAudioSystemProperties(
          activeSpeakerFakeAudioSystemProperties);
      await flushTasks();

      assertEquals(
          fakeCrosAudioConfig.fakeSpeakerActive.id,
          BigInt(outputDeviceDropdown.value));
      assertEquals(
          activeSpeakerFakeAudioSystemProperties.outputDevices.length,
          outputDeviceDropdown.length);
    });

    test('simulate setting active output device', async () => {
      // Get dropdown.
      const outputDeviceDropdown =
          audioPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#audioOutputDeviceDropdown');
      assertTrue(!!outputDeviceDropdown);

      // Verify selected is active device.
      const expectedInitialSelectionId =
          `${fakeCrosAudioConfig.defaultFakeMicJack.id}`;
      assertEquals(expectedInitialSelectionId, outputDeviceDropdown.value);

      // change active device.
      outputDeviceDropdown.selectedIndex = 0;
      outputDeviceDropdown.dispatchEvent(
          new CustomEvent('change', {bubbles: true}));
      await flushTasks();

      // Verify selected updated to latest active device.
      const expectedUpdatedSelectionId =
          `${fakeCrosAudioConfig.defaultFakeSpeaker.id}`;
      assertEquals(expectedUpdatedSelectionId, outputDeviceDropdown.value);
      const nextActiveDevice =
          audioPage.get('audioSystemProperties_')
              .outputDevices.find(
                  (device: crosAudioConfigMojom.AudioDevice) =>
                      device.id === fakeCrosAudioConfig.defaultFakeSpeaker.id);
      assertTrue(nextActiveDevice.isActive);
    });

    test('input device mojo test', async () => {
      const inputDeviceDropdown =
          audioPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#audioInputDeviceDropdown');
      assertTrue(!!inputDeviceDropdown);

      // Test default properties.
      assertEquals(
          `${fakeCrosAudioConfig.fakeInternalFrontMic.id}`,
          inputDeviceDropdown.value);
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties.inputDevices
              .length,
          inputDeviceDropdown.length);

      // Test empty input devices case.
      crosAudioConfig.setAudioSystemProperties(
          emptyInputDevicesFakeAudioSystemProperties);
      await flushTasks();
      assertEquals('', inputDeviceDropdown.value);
      assertEquals(
          emptyInputDevicesFakeAudioSystemProperties.inputDevices.length,
          inputDeviceDropdown.length);

      // If the input devices are empty, the input section should be hidden.
      const inputDeviceSection = audioPage.shadowRoot!.querySelector('#input');
      assertFalse(isVisible(inputDeviceSection));
      const outputDeviceSection =
          audioPage.shadowRoot!.querySelector('#output');
      assertTrue(isVisible(outputDeviceSection));
    });

    test('simulate setting active input device', async () => {
      // Get dropdown.
      const inputDeviceDropdown =
          audioPage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#audioInputDeviceDropdown');
      assertTrue(!!inputDeviceDropdown);

      // Verify selected is active device.
      const expectedInitialSelectionId =
          `${fakeCrosAudioConfig.fakeInternalFrontMic.id}`;
      assertEquals(expectedInitialSelectionId, inputDeviceDropdown.value);

      // change active device.
      inputDeviceDropdown.selectedIndex = 1;
      inputDeviceDropdown.dispatchEvent(
          new CustomEvent('change', {bubbles: true}));
      await flushTasks();

      // Verify selected updated to latest active device.
      const expectedUpdatedSelectionId =
          `${fakeCrosAudioConfig.fakeBluetoothMic.id}`;
      assertEquals(expectedUpdatedSelectionId, inputDeviceDropdown.value);
      const nextActiveDevice =
          audioPage.get('audioSystemProperties_')
              .inputDevices.find(
                  (device: crosAudioConfigMojom.AudioDevice) =>
                      device.id === fakeCrosAudioConfig.fakeBluetoothMic.id);
      assertTrue(nextActiveDevice.isActive);
    });

    test('simulate mute output', async () => {
      assertEquals(
          crosAudioConfigMojom.MuteState.kNotMuted,
          audioPage.get('audioSystemProperties_').outputMuteState);
      assertFalse(audioPage.get('isOutputMuted_'));

      const outputMuteButton =
          audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
              '#audioOutputMuteButton');
      assertTrue(!!outputMuteButton);
      outputMuteButton.click();
      await flushTasks();

      assertEquals(
          crosAudioConfigMojom.MuteState.kMutedByUser,
          audioPage.get('audioSystemProperties_').outputMuteState);
      assertTrue(audioPage.get('isOutputMuted_'));
      assertEquals('settings20:volume-up-off', outputMuteButton.ironIcon);

      outputMuteButton.click();
      await flushTasks();

      assertEquals(
          crosAudioConfigMojom.MuteState.kNotMuted,
          audioPage.get('audioSystemProperties_').outputMuteState);
      assertFalse(audioPage.get('isOutputMuted_'));
    });

    test('simulate input mute button press test', async () => {
      const inputMuteButton =
          audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
              '#audioInputGainMuteButton');
      assertTrue(!!inputMuteButton);

      assertFalse(audioPage.getIsInputMutedForTest());
      assertEquals('cr:mic', inputMuteButton.ironIcon);

      inputMuteButton.click();
      await flushTasks();

      assertTrue(audioPage.getIsInputMutedForTest());
      assertEquals('settings:mic-off', inputMuteButton.ironIcon);
    });

    test('simulate setting input gain slider', async () => {
      const sliderSelector = '#audioInputGainVolumeSlider';
      const inputSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(sliderSelector);
      assertTrue(!!inputSlider);
      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          inputSlider.value);

      const minimumValue = 0;
      await simulateSliderClicked(inputSlider, minimumValue);

      assertEquals(minimumValue, inputSlider.value);
      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          inputSlider.value);
      const maximumValue = 100;
      await simulateSliderClicked(inputSlider, maximumValue);

      assertEquals(maximumValue, inputSlider.value);
      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          inputSlider.value);
      const middleValue = 50;
      await simulateSliderClicked(inputSlider, middleValue);

      assertEquals(middleValue, inputSlider.value);
      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          inputSlider.value);

      // Ensure value clamps to min.
      inputSlider.value = -1;
      inputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          minimumValue);

      // Ensure value clamps to min.
      inputSlider.value = 101;
      inputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          audioPage.get('audioSystemProperties_').inputGainPercent,
          maximumValue);
    });

    test('simulate noise cancellation', async () => {
      const mockController = new MockController();
      const setNoiseCancellationEnabled = mockController.createFunctionMock(
          crosAudioConfig, 'setNoiseCancellationEnabled');

      const noiseCancellationSubsection =
          audioPage.shadowRoot!.querySelector<HTMLDivElement>(
              '#audioInputNoiseCancellationSubsection');
      const noiseCancellationToggle =
          audioPage.shadowRoot!.querySelector<CrToggleElement>(
              '#audioInputNoiseCancellationToggle');

      assertTrue(!!noiseCancellationSubsection);
      assertTrue(isVisible(noiseCancellationSubsection));
      assertTrue(!!noiseCancellationToggle);
      assertFalse(noiseCancellationToggle.checked);

      await noiseCancellationToggle.click();
      await flushTasks();

      assertTrue(isVisible(noiseCancellationSubsection));
      assertTrue(noiseCancellationToggle.checked);
      assertEquals(
          /* expected_call_count */ 1,
          setNoiseCancellationEnabled['calls_'].length);

      // Clicking on the row should toggle the noise cancellation toggle.
      noiseCancellationSubsection.click();
      assertEquals(
          /* expected_call_count */ 2,
          setNoiseCancellationEnabled['calls_'].length);
      const argsPassedToSetNoiseCancellationEnabled =
          setNoiseCancellationEnabled['calls_'][1];
      assertTrue(!!argsPassedToSetNoiseCancellationEnabled);
      // "setNoiseCancellationEnabled" should have been called with "false"
      // after the row was clicked on.
      assertFalse(argsPassedToSetNoiseCancellationEnabled[0]);

      crosAudioConfig.setAudioSystemProperties(
          noiseCancellationNotSupportedAudioSystemProperties);
      await flushTasks();

      assertFalse(isVisible(noiseCancellationSubsection));
    });

    test('simulate style transfer', async () => {
      const mockController = new MockController();
      const setStyleTransferEnabled = mockController.createFunctionMock(
          crosAudioConfig, 'setStyleTransferEnabled');
      crosAudioConfig.setAudioSystemProperties(
          styleTransferSupportedAudioSystemProperties);

      const styleTransferSubsection =
          audioPage.shadowRoot!.querySelector<HTMLDivElement>(
              '#audioInputStyleTransferSubsection');
      const styleTransferToggle =
          audioPage.shadowRoot!.querySelector<CrToggleElement>(
              '#audioInputStyleTransferToggle');

      assertTrue(!!styleTransferSubsection);
      assertTrue(isVisible(styleTransferSubsection));
      assertTrue(!!styleTransferToggle);
      assertFalse(styleTransferToggle.checked);

      await styleTransferToggle.click();
      await flushTasks();

      assertTrue(isVisible(styleTransferSubsection));
      assertTrue(styleTransferToggle.checked);
      assertEquals(
          /* expected_call_count */ 1,
          setStyleTransferEnabled['calls_'].length);

      // Clicking on the row should toggle the style transfer toggle.
      styleTransferSubsection.click();
      assertEquals(
          /* expected_call_count */ 2,
          setStyleTransferEnabled['calls_'].length);
      const argsPassedToSetStyleTransferEnabled =
          setStyleTransferEnabled['calls_'][1];
      assertTrue(!!argsPassedToSetStyleTransferEnabled);
      // "setStyleTransferEnabled" should have been called with "false"
      // after the row was clicked on.
      assertFalse(argsPassedToSetStyleTransferEnabled[0]);

      crosAudioConfig.setAudioSystemProperties(
          styleTransferNotSupportedAudioSystemProperties);
      await flushTasks();

      assertFalse(isVisible(styleTransferSubsection));
    });

    test(
        'simulate hfp mic sr with flag off and unsupported state', async () => {
          const audioHfpMicSrSubsection =
              audioPage.shadowRoot!.querySelector<HTMLElement>(
                  '#audioInputHfpMicSrSubsection');
          const audioInputHfpMicSrToggle =
              audioPage.shadowRoot!.querySelector<CrToggleElement>(
                  '#audioInputHfpMicSrToggle');

          // default
          assertTrue(!!audioHfpMicSrSubsection);
          assertTrue(audioHfpMicSrSubsection.hidden);
          assertTrue(!!audioInputHfpMicSrToggle);
          assertFalse(audioInputHfpMicSrToggle.checked);

          // toggle flag off && not supported
          setEnableAudioHfpMicSRToggleEnabled(false);
          await init();
          crosAudioConfig.setAudioSystemProperties(
              hfpMicSrNotSupportedAudioSystemProperties);
          await flushTasks();

          assertTrue(!!audioHfpMicSrSubsection);
          assertTrue(audioHfpMicSrSubsection.hidden);
          assertFalse(audioInputHfpMicSrToggle.checked);
        });

    test('simulate hfp mic sr with flag on and unsupported state', async () => {
      const audioHfpMicSrSubsection =
          audioPage.shadowRoot!.querySelector<HTMLElement>(
              '#audioInputHfpMicSrSubsection');

      setEnableAudioHfpMicSRToggleEnabled(true);
      await init();
      crosAudioConfig.setAudioSystemProperties(
          hfpMicSrNotSupportedAudioSystemProperties);
      await flushTasks();

      assertTrue(!!audioHfpMicSrSubsection);
      assertTrue(audioHfpMicSrSubsection.hidden);
    });

    test('simulate hfp mic sr with flag off and supported state', async () => {
      const audioHfpMicSrSubsection =
          audioPage.shadowRoot!.querySelector<HTMLElement>(
              '#audioInputHfpMicSrSubsection');

      setEnableAudioHfpMicSRToggleEnabled(false);
      await init();
      crosAudioConfig.setAudioSystemProperties(
          hfpMicSrSupportedAudioSystemProperties);
      await flushTasks();

      assertTrue(!!audioHfpMicSrSubsection);
      assertTrue(audioHfpMicSrSubsection.hidden);
    });

    test('simulate hfp mic sr with flag on and supported state', async () => {
      const audioHfpMicSrSubsection =
          audioPage.shadowRoot!.querySelector<HTMLElement>(
              '#audioInputHfpMicSrSubsection');
      const audioInputHfpMicSrToggle =
          audioPage.shadowRoot!.querySelector<CrToggleElement>(
              '#audioInputHfpMicSrToggle');

      setEnableAudioHfpMicSRToggleEnabled(true);
      await init();
      crosAudioConfig.setAudioSystemProperties(
          hfpMicSrSupportedAudioSystemProperties);
      await flushTasks();

      assertTrue(!!audioHfpMicSrSubsection);
      assertFalse(audioHfpMicSrSubsection.hidden);
      assertTrue(!!audioInputHfpMicSrToggle);
      assertFalse(audioInputHfpMicSrToggle.checked);
    });

    test(
        'simulate hfp mic sr with active device and enabled state',
        async () => {
          setEnableAudioHfpMicSRToggleEnabled(true);
          await init();
          crosAudioConfig.setAudioSystemProperties(
              hfpMicSrSupportedAudioSystemProperties);
          await flushTasks();

          const audioHfpMicSrSubsection =
              audioPage.shadowRoot!.querySelector<HTMLElement>(
                  '#audioInputHfpMicSrSubsection');
          const audioInputHfpMicSrToggle =
              audioPage.shadowRoot!.querySelector<CrToggleElement>(
                  '#audioInputHfpMicSrToggle');

          // default not enabled
          assertTrue(!!audioHfpMicSrSubsection);
          assertFalse(audioHfpMicSrSubsection.hidden);
          assertTrue(!!audioInputHfpMicSrToggle);
          assertFalse(audioInputHfpMicSrToggle.checked);

          // clicks the toggle
          await audioInputHfpMicSrToggle.click();
          await flushTasks();

          const micId =
              fakeCrosAudioConfig.fakeBluetoothNbMicActiveHfpMicSrNotEnabled.id;
          assertTrue(crosAudioConfig.isHfpMicSrEnabled(micId));
          assertFalse(audioHfpMicSrSubsection.hidden);
          assertTrue(audioInputHfpMicSrToggle.checked);

          // clicks the toggle again
          await audioInputHfpMicSrToggle.click();
          await flushTasks();

          assertFalse(crosAudioConfig.isHfpMicSrEnabled(micId));
          assertFalse(audioHfpMicSrSubsection.hidden);
          assertFalse(audioInputHfpMicSrToggle.checked);

          // selects other input device that doesn't support the feature
          crosAudioConfig.setActiveDevice(
              fakeCrosAudioConfig.fakeMicJackInactive.id);
          await flushTasks();

          assertTrue(audioHfpMicSrSubsection.hidden);
        });

    test('simulate input muted by hardware', async () => {
      const muteSelector = '#audioInputGainMuteButton';
      const inputMuteButton =
          audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
              muteSelector);
      const sliderSelector = '#audioInputGainVolumeSlider';
      const inputSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(sliderSelector);
      assertTrue(!!inputMuteButton);
      assertFalse(inputMuteButton.disabled);
      assertTrue(!!inputSlider);
      assertFalse(inputSlider.disabled);
      assertFalse(audioPage.getIsInputMutedForTest());

      crosAudioConfig.setAudioSystemProperties(
          muteByHardwareAudioSystemProperties);
      await flushTasks();

      assertTrue(inputMuteButton.disabled);
      assertTrue(inputSlider.disabled);
      assertTrue(audioPage.getIsInputMutedForTest());
    });

    test('simulate output mute-by-policy', async () => {
      const enterpriseIconSelector = '#audioOutputMuteByPolicyIndicator';
      assertFalse(isVisible(
          audioPage.shadowRoot!.querySelector(enterpriseIconSelector)));
      const outputMuteButtonSelector = '#audioOutputMuteButton';
      const outputMuteButton =
          audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
              outputMuteButtonSelector);
      assertTrue(!!outputMuteButton);
      assertFalse(outputMuteButton.disabled);
      const outputSliderSelector = '#outputVolumeSlider';
      const outputSlider = audioPage.shadowRoot!.querySelector<CrSliderElement>(
          outputSliderSelector);
      assertTrue(!!outputSlider);
      assertFalse(outputSlider.disabled);

      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();

      assertTrue(isVisible(
          audioPage.shadowRoot!.querySelector(enterpriseIconSelector)));
      assertTrue(outputMuteButton.disabled);
      assertTrue(outputSlider.disabled);
    });

    test('noise cancellation after system properties change', async () => {
      // System properties change should not trigger setNoiseCancellationEnabled
      // to be called.
      const mockController = new MockController();
      const setNoiseCancellationEnabled = mockController.createFunctionMock(
          crosAudioConfig, 'setNoiseCancellationEnabled');

      assertEquals(
          /* expected_call_count */ 0,
          setNoiseCancellationEnabled['calls_'].length);

      crosAudioConfig.setAudioSystemProperties(
          {...fakeCrosAudioConfig.defaultFakeAudioSystemProperties});
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 0,
          setNoiseCancellationEnabled['calls_'].length);

      crosAudioConfig.setAudioSystemProperties(
          noiseCancellationNotSupportedAudioSystemProperties);
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 0,
          setNoiseCancellationEnabled['calls_'].length);
    });

    test('style transfer after system properties change', async () => {
      // System properties change should not trigger setStyleTransferEnabled
      // to be called.
      const mockController = new MockController();
      const setStyleTransferEnabled = mockController.createFunctionMock(
          crosAudioConfig, 'setStyleTransferEnabled');

      assertEquals(
          /* expected_call_count */ 0,
          setStyleTransferEnabled['calls_'].length);

      crosAudioConfig.setAudioSystemProperties(
          styleTransferSupportedAudioSystemProperties);
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 0,
          setStyleTransferEnabled['calls_'].length);

      crosAudioConfig.setAudioSystemProperties(
          styleTransferNotSupportedAudioSystemProperties);
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 0,
          setStyleTransferEnabled['calls_'].length);
    });

    test('slider keypress correct increments', () => {
      // The audio sliders are expected to increment in intervals of 10.
      const outputVolumeSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(
              '#outputVolumeSlider');
      assertTrue(!!outputVolumeSlider);
      assertEquals(75, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(85, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(95, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(100, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(100, outputVolumeSlider.value);
      pressArrowLeft(outputVolumeSlider);
      assertEquals(90, outputVolumeSlider.value);
      pressArrowLeft(outputVolumeSlider);
      assertEquals(80, outputVolumeSlider.value);

      const inputVolumeSlider =
          audioPage.shadowRoot!.querySelector<CrSliderElement>(
              '#audioInputGainVolumeSlider');
      assertTrue(!!inputVolumeSlider);
      assertEquals(87, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(97, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(100, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(100, inputVolumeSlider.value);
      pressArrowLeft(inputVolumeSlider);
      assertEquals(90, inputVolumeSlider.value);
      pressArrowLeft(inputVolumeSlider);
      assertEquals(80, inputVolumeSlider.value);
    });

    test('mute state updates tooltips', async () => {
      const outputMuteTooltip =
          audioPage.shadowRoot!.querySelector('#audioOutputMuteButtonTooltip');
      const inputMuteTooltip =
          audioPage.shadowRoot!.querySelector('#audioInputMuteButtonTooltip');
      assertTrue(!!outputMuteTooltip);
      assertTrue(!!inputMuteTooltip);

      // Default state should be unmuted so show the toggle mute tooltip.
      assertEquals(
          loadTimeData.getString('audioToggleToMuteTooltip'),
          outputMuteTooltip.textContent!.trim());
      assertEquals(
          loadTimeData.getString('audioToggleToMuteTooltip'),
          inputMuteTooltip.textContent!.trim());

      // Test muted by user case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByUserFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioToggleToUnmuteTooltip'),
          outputMuteTooltip.textContent!.trim());
      assertEquals(
          loadTimeData.getString('audioToggleToUnmuteTooltip'),
          inputMuteTooltip.textContent!.trim());

      // Test muted by policy case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioMutedByPolicyTooltip'),
          outputMuteTooltip.textContent!.trim());
      assertEquals(
          loadTimeData.getString('audioMutedByPolicyTooltip'),
          inputMuteTooltip.textContent!.trim());

      // Test muted externally case.
      crosAudioConfig.setAudioSystemProperties(
          mutedExternallyFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioMutedExternallyTooltip'),
          outputMuteTooltip.textContent!.trim());
      assertEquals(
          loadTimeData.getString('audioMutedExternallyTooltip'),
          inputMuteTooltip.textContent!.trim());
    });

    test(
        'mute state updates button aria-description and aria-pressed',
        async () => {
          const outputMuteButton =
              audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
                  '#audioOutputMuteButton');
          const inputMuteButton =
              audioPage.shadowRoot!.querySelector<CrIconButtonElement>(
                  '#audioInputGainMuteButton');
          assertTrue(!!outputMuteButton);
          assertTrue(!!inputMuteButton);

          // Default state should be unmuted so show the toggle mute tooltip.
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelNotMuted'),
              outputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelNotMuted'),
              inputMuteButton.getAttribute('aria-description')!.trim());
          const ariaNotPressedValue = 'false';
          assertEquals(ariaNotPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaNotPressedValue, inputMuteButton.ariaPressed);

          // Test muted by user case.
          crosAudioConfig.setAudioSystemProperties(
              mutedByUserFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelMuted'),
              inputMuteButton.getAttribute('aria-description')!.trim());
          const ariaPressedValue = 'true';
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);

          // Test muted by policy case.
          crosAudioConfig.setAudioSystemProperties(
              mutedByPolicyFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelMuted'),
              inputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);

          // Test muted externally case.
          crosAudioConfig.setAudioSystemProperties(
              mutedExternallyFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(
              loadTimeData.getString(
                  'audioInputMuteButtonAriaLabelMutedByHardwareSwitch'),
              inputMuteButton.getAttribute('aria-description')!.trim());
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);
        });
  });

  if (!isRevampWayfindingEnabled) {
    // When the revamp is enabled, the power settings exist under the
    // System Preferences page.
    suite('power', () => {
      setup(async () => {
        await init();
      });

      test('power subpage visibility', () => {
        const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
            `#main #powerRow`);
        assertTrue(!!row);
        row.click();
        assertEquals(routes.POWER, Router.getInstance().currentRoute);
        const powerPage =
            devicePage.shadowRoot!.querySelector('settings-power');
        assertTrue(!!powerPage);
      });
    });
  }

  suite('keyboard subpage', () => {
    function queryKeyboardRow(): HTMLElement|null {
      return devicePage.shadowRoot!.querySelector('#keyboardRow');
    }

    test('Keyboard row is not visible if device split is enabled', async () => {
      setDeviceSplitEnabled(true);

      await init();
      assertFalse(isVisible(queryKeyboardRow()));
    });

    test('Keyboard row is visible if device split is disabled', async () => {
      setDeviceSplitEnabled(false);

      await init();
      assertTrue(isVisible(queryKeyboardRow()));
    });

    test('Clicking keyboard row goes to keyboard subpage', async () => {
      setDeviceSplitEnabled(false);
      await init();

      const keyboardRow = queryKeyboardRow();
      assertTrue(!!keyboardRow);
      keyboardRow.click();

      assertEquals(routes.KEYBOARD, Router.getInstance().currentRoute);
      const subpage = devicePage.shadowRoot!.querySelector<HTMLElement>(
          'settings-keyboard');
      assertTrue(!!subpage);
    });
  });

  suite('per-device-keyboard subpage', () => {
    let perDeviceKeyboardPage: SettingsPerDeviceKeyboardElement;
    let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeKeyboards(fakeKeyboards);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async () => {
      await init();
      const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
          `#main #perDeviceKeyboardRow`);
      assertTrue(!!row);
      row.click();
      assertEquals(
          routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
      const page =
          devicePage.shadowRoot!.querySelector('settings-per-device-keyboard');
      assertTrue(!!page);
      perDeviceKeyboardPage = page;
    });

    teardown(() => {
      perDeviceKeyboardPage.remove();
    });

    test('per-device keyboard subpage visibility', () => {
      assertEquals(
          routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
    });

    test('per-device keyboard page populated', () => {
      const connectedKeyboards = perDeviceKeyboardPage.get('keyboards');
      assertTrue(!!connectedKeyboards);
      assertDeepEquals(fakeKeyboards, connectedKeyboards);
    });
  });

  suite('pointers subpage', () => {
    function queryPointersRow(): HTMLElement|null {
      return devicePage.shadowRoot!.querySelector('#pointersRow');
    }

    test('Pointers row is not visible if device split is enabled', async () => {
      setDeviceSplitEnabled(true);

      await init();
      assertFalse(isVisible(queryPointersRow()));
    });

    test('Pointers row is visible if device split is disabled', async () => {
      setDeviceSplitEnabled(false);

      await init();
      assertTrue(isVisible(queryPointersRow()));
    });

    test('Clicking pointers row goes to pointers subpage', async () => {
      setDeviceSplitEnabled(false);
      await init();

      const pointersRow = queryPointersRow();
      assertTrue(!!pointersRow);
      pointersRow.click();

      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      const subpage = devicePage.shadowRoot!.querySelector<HTMLElement>(
          'settings-pointers');
      assertTrue(!!subpage);
    });
  });

  suite('per-device mouse', () => {
    let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeMice(fakeMice);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async () => {
      await init();
    });

    test('per-device mouse subpage visibility', () => {
      const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
          `#main #perDeviceMouseRow`);
      assertTrue(!!row);
      row.click();
      assertEquals(routes.PER_DEVICE_MOUSE, Router.getInstance().currentRoute);
      const perDeviceMousePage =
          devicePage.shadowRoot!.querySelector('settings-per-device-mouse');
      assertTrue(!!perDeviceMousePage);
    });
  });

  suite('per-device touchpad', () => {
    let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeTouchpads(fakeTouchpads);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async () => {
      await init();
    });

    test('per-device touchpad subpage visibility', () => {
      const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
          `#main #perDeviceTouchpadRow`);
      assertTrue(!!row);
      row.click();
      assertEquals(
          routes.PER_DEVICE_TOUCHPAD, Router.getInstance().currentRoute);
      const perDeviceTouchpadPage =
          devicePage.shadowRoot!.querySelector('settings-per-device-touchpad');
      assertTrue(!!perDeviceTouchpadPage);
    });
  });

  suite('per-device pointing stick', () => {
    let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakePointingSticks(fakePointingSticks);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async () => {
      await init();
    });

    test('per-device pointing stick subpage visibility', () => {
      const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
          `#main #perDevicePointingStickRow`);
      assertTrue(!!row);
      row.click();
      assertEquals(
          routes.PER_DEVICE_POINTING_STICK, Router.getInstance().currentRoute);
      const perDevicePointingStickPage = devicePage.shadowRoot!.querySelector(
          'settings-per-device-pointing-stick');
      assertTrue(!!perDevicePointingStickPage);
    });
  });

  suite('stylus subpage', () => {
    test('Clicking stylus row goes to stylus subpage', async () => {
      await init();

      const stylusRow =
          devicePage.shadowRoot!.querySelector<HTMLElement>('#stylusRow');
      assertTrue(!!stylusRow);
      stylusRow.click();

      assertEquals(routes.STYLUS, Router.getInstance().currentRoute);
      const subpage =
          devicePage.shadowRoot!.querySelector<HTMLElement>('settings-stylus');
      assertTrue(!!subpage);
    });
  });

  if (isRevampWayfindingEnabled) {
    test('Power row is not visible', async () => {
      await init();
      const powerRow = devicePage.shadowRoot!.querySelector('#powerRow');
      assertFalse(isVisible(powerRow));
    });

    test('Storage row is not visible', async () => {
      await init();
      const storageRow = devicePage.shadowRoot!.querySelector('#storageRow');
      assertFalse(isVisible(storageRow));
    });

    test('Printing settings card is visible', async () => {
      await init();
      const printingSettingsCard =
          devicePage.shadowRoot!.querySelector('printing-settings-card');
      assertTrue(isVisible(printingSettingsCard));
    });
  }
});
