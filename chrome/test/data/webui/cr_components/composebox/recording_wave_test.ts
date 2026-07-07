// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/search/recording_wave.js';

import {AudioProcessor} from 'chrome://resources/cr_components/search/audio_processor.service.js';
import type {RecordingWaveElement} from 'chrome://resources/cr_components/search/recording_wave.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('RecordingWaveElementTest', () => {
  let recordingWaveElement: RecordingWaveElement;
  let originalMatchMedia: typeof window.matchMedia;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    recordingWaveElement = document.createElement('recording-wave');
    recordingWaveElement.style.display = 'block';
    recordingWaveElement.style.width = '1500px';
    document.body.appendChild(recordingWaveElement);
    await microtasksFinished();

    originalMatchMedia = window.matchMedia;
  });

  teardown(() => {
    recordingWaveElement.remove();
    window.matchMedia = originalMatchMedia;
  });

  function setPrefersColorSchemeDark(isDark: boolean) {
    window.matchMedia = (query: string) => {
      return {
        matches: query.includes('dark') && isDark,
        media: query,
        onchange: null,
        addListener: () => {},
        removeListener: () => {},
        addEventListener: () => {},
        removeEventListener: () => {},
        dispatchEvent: () => false,
      } as unknown as MediaQueryList;
    };
  }

  test('initial properties and tag name', () => {
    assertEquals('RECORDING-WAVE', recordingWaveElement.tagName);
    assertFalse(recordingWaveElement.isListening);
    assertTrue(recordingWaveElement.darkThemeColorsEnabled);
  });

  test('isListening toggles internal state and AudioProcessor', async () => {
    let startCalled = false;
    let stopCalled = false;
    const origStart = AudioProcessor.startMonitoringLevels;
    const origStop = AudioProcessor.stopListening;

    AudioProcessor.startMonitoringLevels = () => {
      startCalled = true;
      return Promise.resolve(true);
    };
    AudioProcessor.stopListening = () => {
      stopCalled = true;
    };

    try {
      recordingWaveElement.isListening = true;
      await recordingWaveElement.updateComplete;
      await microtasksFinished();

      assertTrue(startCalled);
      assertTrue((recordingWaveElement as any).animationFrameId_ !== null);
      const pills =
          recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
      assertTrue(pills.length > 0);

      recordingWaveElement.isListening = false;
      await recordingWaveElement.updateComplete;
      await microtasksFinished();

      assertTrue(stopCalled);
      assertEquals(null, (recordingWaveElement as any).animationFrameId_);
      assertEquals(0, recordingWaveElement.$.barsContainer.children.length);
    } finally {
      AudioProcessor.startMonitoringLevels = origStart;
      AudioProcessor.stopListening = origStop;
    }
  });

  test(
      'dark theme colors are shown if prefers-color-scheme dark and darkThemeColorsEnabled is true',
      async () => {
        setPrefersColorSchemeDark(true);
        recordingWaveElement.darkThemeColorsEnabled = true;
        recordingWaveElement.isListening = true;
        await recordingWaveElement.updateComplete;
        await microtasksFinished();

        // Spawn all bars to set their colors.
        const barsData = (recordingWaveElement as any).barsData_;
        barsData.forEach((bar: any) => {
          bar.isUnspawned = false;
        });

        // Wait for animation frame to apply styles.
        await new Promise(resolve => requestAnimationFrame(resolve));

        const pills =
            recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
        assertEquals(100, pills.length);
        const lastPill = pills[99] as HTMLElement;

        // Dark stop for ratio 1.0 is rgb(55, 70, 109).
        assertEquals('rgb(55, 70, 109)', lastPill.style.background);

        // Verify that the CSS variable is set to the dark theme color.
        const computedStyle = getComputedStyle(recordingWaveElement);
        assertEquals(
            '#37466d',
            computedStyle.getPropertyValue('--color-recording-wave').trim());
      });

  test(
      'dark theme colors are not shown if darkThemeColorsEnabled is false',
      async () => {
        setPrefersColorSchemeDark(true);
        recordingWaveElement.darkThemeColorsEnabled = false;
        recordingWaveElement.isListening = true;
        await recordingWaveElement.updateComplete;
        await microtasksFinished();

        // Spawn all bars to set their colors.
        const barsData = (recordingWaveElement as any).barsData_;
        barsData.forEach((bar: any) => {
          bar.isUnspawned = false;
        });

        // Wait for animation frame to apply styles.
        await new Promise(resolve => requestAnimationFrame(resolve));

        const pills =
            recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
        assertEquals(100, pills.length);
        const lastPill = pills[99] as HTMLElement;

        // Light stop for ratio 1.0 is rgb(236, 240, 255).
        assertEquals('rgb(236, 240, 255)', lastPill.style.background);

        // Verify that the CSS variable is NOT set to dark color.
        const computedStyle = getComputedStyle(recordingWaveElement);
        assertEquals(
            '#c9d2ff',
            computedStyle.getPropertyValue('--color-recording-wave').trim());
      });

  test('minimum to maximum height scaling logic', async () => {
    const originalGetVolume = AudioProcessor.getVolume;
    let mockVolume = 0;
    AudioProcessor.getVolume = () => mockVolume;

    try {
      recordingWaveElement.isListening = true;
      await recordingWaveElement.updateComplete;
      await microtasksFinished();

      const barsData = (recordingWaveElement as any).barsData_;

      const testVolumeMapping = (volume: number, expectedHeightPx: number) => {
        mockVolume = volume;
        // ACTIVATION_DELAY_INDEX is 6.
        barsData[6].isUnspawned = true;

        // Manually trigger animation loop frame logic.
        (recordingWaveElement as any).animationLoop_(performance.now());

        assertEquals(expectedHeightPx, barsData[6].targetHeightPx);
      };

      testVolumeMapping(0, 8);     // MINIMUM_BAR_HEIGHT
      testVolumeMapping(1, 36);    // MAX_BAR_HEIGHT
      testVolumeMapping(0.5, 22);  // Mid-point
    } finally {
      AudioProcessor.getVolume = originalGetVolume;
    }
  });

  test('small noise under 0.02 volume threshold is hidden', async () => {
    const originalGetVolume = AudioProcessor.getVolume;
    AudioProcessor.getVolume = () => 0.015;  // Under 0.02 threshold

    try {
      recordingWaveElement.isListening = true;
      await recordingWaveElement.updateComplete;
      await microtasksFinished();

      const barsData = (recordingWaveElement as any).barsData_;
      barsData[6].isUnspawned = true;

      (recordingWaveElement as any).animationLoop_(performance.now());

      assertEquals(0, barsData[6].level);
      assertEquals(8, barsData[6].targetHeightPx);
    } finally {
      AudioProcessor.getVolume = originalGetVolume;
    }
  });

  test('resize observer updates maxBars_', async () => {
    // 300px width should yield 300 / 15 = 20 bars.
    recordingWaveElement.style.width = '300px';
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();
    assertEquals(20, (recordingWaveElement as any).maxBars_);

    // 150px width should yield 150 / 15 = 10 bars.
    recordingWaveElement.style.width = '150px';
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();
    assertEquals(10, (recordingWaveElement as any).maxBars_);
  });

  test('animation loop shrinks or grows bars dynamically', async () => {
    recordingWaveElement.style.width = '300px';
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();

    recordingWaveElement.isListening = true;
    await recordingWaveElement.updateComplete;
    await microtasksFinished();

    let pills =
        recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
    assertEquals(20, pills.length);
    assertEquals(20, (recordingWaveElement as any).barsData_.length);

    // Shrink width to 150px (10 bars).
    recordingWaveElement.style.width = '150px';
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();
    (recordingWaveElement as any).animationLoop_();

    pills = recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
    assertEquals(10, pills.length);
    assertEquals(10, (recordingWaveElement as any).barsData_.length);

    // Grow width back to 300px (20 bars).
    recordingWaveElement.style.width = '300px';
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();
    (recordingWaveElement as any).animationLoop_();

    pills = recordingWaveElement.$.barsContainer.querySelectorAll('.bar-pill');
    assertEquals(20, pills.length);
    assertEquals(20, (recordingWaveElement as any).barsData_.length);
  });

  test('disconnected callback cleans up resources', async () => {
    let stopCalled = false;
    const origStop = AudioProcessor.stopListening;
    AudioProcessor.stopListening = () => {
      stopCalled = true;
    };

    try {
      recordingWaveElement.isListening = true;
      await recordingWaveElement.updateComplete;
      await microtasksFinished();

      assertTrue((recordingWaveElement as any).animationFrameId_ !== null);

      recordingWaveElement.remove();
      await microtasksFinished();

      assertTrue(stopCalled);
      assertEquals(null, (recordingWaveElement as any).animationFrameId_);
    } finally {
      AudioProcessor.stopListening = origStop;
    }
  });
});
