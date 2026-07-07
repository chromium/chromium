// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioLevelsProcessor} from 'chrome://resources/cr_components/search/audio_levels.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('AudioLevelsProcessorTest', () => {
  let processor: AudioLevelsProcessor;

  setup(() => {
    processor = new AudioLevelsProcessor();
  });

  test('should return 0 for low RMS values after draining history', () => {
    // Use silence buffer (all -100 dB) to get RMS 0.
    const buffer = new Float32Array(512).fill(-100);
    let level = 0;
    let now = 1000;
    for (let i = 0; i < 100; i++) {
      level = processor.process(buffer, now);
      now += 60;
    }
    assertEquals(0, level);
  });

  test('should return non-zero for high RMS values', () => {
    // Use buffer with values in target band to produce non-zero volume.
    const buffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      buffer[i] = -50;
    }
    const level = processor.process(buffer, 1000);
    assertTrue(level > 0);
  });

  test('should smooth values', () => {
    // Call multiple times to see smoothing effect
    const buffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      buffer[i] = -50;
    }
    const levels: number[] = [];
    let now = 1000;
    for (let i = 0; i < 10; i++) {
      levels.push(processor.process(buffer, now));
      now += 60;
    }
    // Check that values are not all identical, showing adaptation/smoothing
    const uniqueLevels = new Set(levels);
    assertTrue(uniqueLevels.size > 1);
  });

  test('should handle rapid drops in level', () => {
    const lowBuffer = new Float32Array(512).fill(-100);
    const highBuffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      highBuffer[i] = -50;
    }
    let now = 1000;
    processor.process(highBuffer, now);
    now += 60;
    processor.process(highBuffer, now);
    now += 60;
    const levelDrop = processor.process(lowBuffer, now);
    // Should not drop to 0 immediately due to history/smoothing
    assertTrue(levelDrop > 0);
  });

  test('should cover solveCubic loop', () => {
    const buffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      buffer[i] = -50;
    }

    let now = 1000;
    for (let i = 0; i < 30; i++) {
      processor.process(buffer, now);
      now += 60;
    }

    const level = processor.process(buffer, now);
    assertTrue(level > 0);
  });

  test(
      'should handle extremely low SNR (calculateSpeechLevel edge case)',
      () => {
        const highBuffer = new Float32Array(512).fill(-100);
        for (let i = 7; i <= 70; i++) {
          highBuffer[i] = 40;
        }

        let now = 1000;
        for (let i = 0; i < 200; i++) {
          processor.process(highBuffer, now);
          now += 60;
        }

        const silenceBuffer = new Float32Array(512).fill(-100);
        const level = processor.process(silenceBuffer, now);

        assertTrue(level < 0.5);
      });

  test('should react quickly to speech after silence', () => {
    const silenceBuffer = new Float32Array(512).fill(-100);
    let now = 1000;
    for (let i = 0; i < 20; i++) {
      processor.process(silenceBuffer, now);
      now += 60;
    }

    const speechBuffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      speechBuffer[i] = -50;
    }

    const levels: number[] = [];
    for (let i = 0; i < 5; i++) {
      levels.push(processor.process(speechBuffer, now));
      now += 60;
    }

    // We want it to react quickly, let's check if it exceeds 0.1 by the 3rd
    // frame.
    assertTrue(levels[2]! > 0.1);
  });

  test('should react to speech immediately after init', () => {
    const speechBuffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      speechBuffer[i] = -50;
    }

    const levels: number[] = [];
    let now = 1000;
    for (let i = 0; i < 5; i++) {
      levels.push(processor.process(speechBuffer, now));
      now += 60;
    }

    assertTrue(levels[0]! > 0.05);
  });

  test(
      'should handle moderate noise immediately with default init noise',
      () => {
        const noiseBuffer = new Float32Array(512).fill(-100);
        for (let i = 7; i <= 70; i++) {
          noiseBuffer[i] = -60;
        }
        const level = processor.process(noiseBuffer, 1000);
        assertEquals(0, level);
      });

  test(
      'should return last emitted value when process is called' +
          'within sampling interval',
      () => {
        const buffer = new Float32Array(512).fill(-100);
        for (let i = 7; i <= 70; i++) {
          buffer[i] = -50;
        }
        const firstLevel = processor.process(buffer, 1000);
        // Call again 20ms later (less than sampleIntervalMs of 60ms)
        const secondLevel = processor.process(buffer, 1020);
        assertEquals(firstLevel, secondLevel);
      });

  test('should ignore frequencies outside speech band (bins 7 to 70)', () => {
    const outOfBandBuffer = new Float32Array(512).fill(-100);
    // Put high energy in bins outside 7-70
    for (let i = 0; i < 7; i++) {
      outOfBandBuffer[i] = 0;
    }
    for (let i = 71; i < 512; i++) {
      outOfBandBuffer[i] = 0;
    }
    const level = processor.process(outOfBandBuffer, 1000);
    assertEquals(0, level);
  });

  test('should handle custom dynamic shaping curve min second value', () => {
    const customProcessor = new AudioLevelsProcessor(0.8);
    const speechBuffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      speechBuffer[i] = -50;
    }
    const level = customProcessor.process(speechBuffer, 1000);
    assertTrue(level > 0);
  });

  test('should handle large time jumps cleanly', () => {
    const buffer = new Float32Array(512).fill(-100);
    for (let i = 7; i <= 70; i++) {
      buffer[i] = -50;
    }
    processor.process(buffer, 1000);
    // Jump forward by 1000ms (> 2 * sampleIntervalMs)
    const levelAfterJump = processor.process(buffer, 2000);
    assertTrue(levelAfterJump >= 0 && levelAfterJump <= 1);
  });

  test('should handle extremely low dB values without NaN', () => {
    const buffer = new Float32Array(512).fill(-Infinity);
    const level = processor.process(buffer, 1000);
    assertEquals(0, level);
  });
});
