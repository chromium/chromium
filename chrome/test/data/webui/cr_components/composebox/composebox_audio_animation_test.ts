// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/search/audio_wave.js';

import type {AudioWaveElement, Bump} from 'chrome://resources/cr_components/search/audio_wave.js';
import {bezierEasing, weightedAverage} from 'chrome://resources/cr_components/search/audio_wave.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAlmostEquals} from './composebox_test_utils.js';

type MockAudioWaveElement = Omit<
    AudioWaveElement,
    |'isExpanding_'|'animationFrameId_'|'decayingAmplitude_'|'lastWordCount_'|
    'containerWidth_'|'volumeHistory_'|'activeSimulatedBumps_'|'frame_'|
    'onStartListen_'|'updateVolume_'|'makeSimulatedAudioBump_'|'firstSyllable_'|
    'getSimulatedAudioBumpsSum_'>&{
  isExpanding_: boolean,
  animationFrameId_: number,
  decayingAmplitude_: number,
  lastWordCount_: number,
  containerWidth_: number,
  volumeHistory_: number[],
  activeSimulatedBumps_: Bump[],
  frame_: number,
  firstSyllable_: boolean,
  onStartListen_: () => void,
  updateVolume_: () => void,
  makeSimulatedAudioBump_:
      (durationMultiplier: number, durationOffset: number, startTime: number,
       maxVolMultiplier: number, maxVolOffset: number) => void,
  getSimulatedAudioBumpsSum_: () => number,
};

suite('Composebox audio wave animation', () => {
  let audioWaveElement: MockAudioWaveElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    /* To avoid typescript from seeing the types as different due to the
     * specific omitted protected methods being public in the mock type.
     */
    const initialElement: AudioWaveElement =
        document.createElement('audio-wave');
    audioWaveElement = initialElement as unknown as MockAudioWaveElement;
    document.body.appendChild(audioWaveElement);

    // Give the audio wave element some dimensions for the ResizeObserver.
    audioWaveElement.style.display = 'block';
    audioWaveElement.style.width = '300px';
    audioWaveElement.style.height = '100px';

    await microtasksFinished();
  });

  test('svg elements are created and mapped', () => {
    // Check that the mapped audioWaveElements in the interface exist.
    assertTrue(!!audioWaveElement.$.mask);
    assertTrue(!!audioWaveElement.$.thinPath);
    assertTrue(!!audioWaveElement.$.lowerGlowPath);
    assertTrue(!!audioWaveElement.$.clipPathShape);
    assertTrue(!!audioWaveElement.$.eclipseSvg);
  });

  test('isListening toggles internal animation state', async () => {
    audioWaveElement.receivedSpeech =
        true;  // Pretend there was a previous session.
    assertFalse(audioWaveElement.isListening);
    assertFalse(audioWaveElement.isExpanding_);

    audioWaveElement.isListening = true;
    // Wait for `updated()` to run.
    await audioWaveElement.updateComplete;
    await microtasksFinished();
    assertTrue(audioWaveElement.isExpanding_);
    assertFalse(
        audioWaveElement.receivedSpeech);  // Set false by `isListening`=true.
    const isSilent = audioWaveElement.volumeHistory_.every(v => v <= 0.001);
    assertTrue(isSilent, 'volumeHistory_ should be reset to silence values');
    assertEquals(5, audioWaveElement.volumeHistory_.length);
    audioWaveElement.volumeHistory_.forEach(val => {
      assertEquals('number', typeof val);
    });

    assertTrue(audioWaveElement.animationFrameId_ !== null);

    audioWaveElement.isListening = false;
    await audioWaveElement.updateComplete;
    assertFalse(audioWaveElement.isExpanding_);
    await microtasksFinished();

    assertEquals(null, audioWaveElement.animationFrameId_);
  });

  test(
      'transcript updates trigger syllable bumps when first noise is not word',
      async () => {
        audioWaveElement.transcript = '';
        audioWaveElement.isListening = true;
        await audioWaveElement.updateComplete;
        await microtasksFinished;

        audioWaveElement.receivedSpeech = true;

        await audioWaveElement.updateComplete;
        await microtasksFinished;

        assertFalse(
            audioWaveElement.firstSyllable_,
            'First syllable should be false since receivedSpeech is true\
            while transcript is empty. This means it is not already heard\
            and accounted for by receivedSpeech. That receivedSpeech\
            indication was simply a random background noise, not speech.');

        const getBumpsLength = () =>
            audioWaveElement.activeSimulatedBumps_.length;
        assertEquals(
            1, getBumpsLength(),
            'There should be 1 bump from receivedSpeech noise');

        audioWaveElement.transcript = 'test';
        await audioWaveElement.updateComplete;
        await microtasksFinished;

        assertEquals(2, getBumpsLength(), 'There should be 1 bump from "test"');

        audioWaveElement.transcript = 'test hello';
        await audioWaveElement.updateComplete;

        assertEquals(4, getBumpsLength(), 'There should be 4 bumps');
      });

  test('transcript updates trigger syllable bumps', async () => {
    audioWaveElement.isListening = true;
    await audioWaveElement.updateComplete;
    await microtasksFinished;

    assertFalse(
        audioWaveElement.receivedSpeech,
        'Received speech should be reset to false once isListening is true');
    audioWaveElement.transcript = 'test';
    audioWaveElement.receivedSpeech = true;
    await audioWaveElement.updateComplete;
    await microtasksFinished;

    /* First syllable becomes true when receivedSpeech Updated() runs,
     * meaning receivedSpeech turning true indicates that what was heard
     * was a word (the first syllable). Transcript will ignore its next word's
     * first syllable. After though, once transcript Updated() runs, it will be
     * false.
     */
    assertFalse(
        audioWaveElement.firstSyllable_,
        'First syllable should be false since first syllable was read by\
        transcript updated()');

    const getBumpsLength = () => audioWaveElement.activeSimulatedBumps_.length;
    await audioWaveElement.updateComplete;
    await microtasksFinished;
    assertEquals(
        1, getBumpsLength(), 'There should be exactly 1 bump fromt "test"');

    audioWaveElement.transcript = 'test hello';
    await audioWaveElement.updateComplete;
    await microtasksFinished;
    assertEquals(3, getBumpsLength(), 'There should be 3 bumps total');
  });

  test('receivedSpeech triggers visual amplitude spike', async () => {
    audioWaveElement.isListening = true;
    await audioWaveElement.updateComplete;

    assertEquals(0, audioWaveElement.decayingAmplitude_);

    audioWaveElement.receivedSpeech = true;
    await audioWaveElement.updateComplete;

    assertTrue(
        audioWaveElement.decayingAmplitude_ >= 0.4,
        `Decaying amplitude too small:\
        ${audioWaveElement.decayingAmplitude_}`);
  });

  test('empty transcript resets word count logic', async () => {
    audioWaveElement.isListening = true;
    audioWaveElement.transcript = 'one two three';
    await audioWaveElement.updateComplete;


    assertEquals(3, audioWaveElement.lastWordCount_);

    audioWaveElement.transcript = '';
    await audioWaveElement.updateComplete;
    assertEquals(0, audioWaveElement.lastWordCount_);
  });

  test('disconnected callback cleans up animation', async () => {
    audioWaveElement.isListening = true;
    await microtasksFinished();

    assertTrue(audioWaveElement.isExpanding_);
    assertTrue(audioWaveElement.animationFrameId_ !== null);

    audioWaveElement.remove();
    await microtasksFinished();

    assertFalse(audioWaveElement.isExpanding_);
    assertEquals(null, audioWaveElement.animationFrameId_);
  });

  test('has correct heuristic syllable counter logic', async () => {
    audioWaveElement.isListening = true;
    await microtasksFinished();

    audioWaveElement.receivedSpeech = true;
    audioWaveElement.transcript = 'hello';
    await microtasksFinished();
    await audioWaveElement.updateComplete;

    const bumpsForHello = audioWaveElement.activeSimulatedBumps_.length;
    assertEquals(2, bumpsForHello, 'Hello syllable number not correct');

    audioWaveElement.transcript = '';
    await microtasksFinished();
    await audioWaveElement.updateComplete;
    audioWaveElement.activeSimulatedBumps_ = [];

    audioWaveElement.transcript = 'chromium';
    await microtasksFinished();
    await audioWaveElement.updateComplete;
    const bumpsForChromium = audioWaveElement.activeSimulatedBumps_.length;

    assertEquals(3, bumpsForChromium, 'Chromium syllable number not correct');
    assertTrue(
        audioWaveElement.activeSimulatedBumps_ &&
        audioWaveElement.activeSimulatedBumps_.length > 0);
    assertTrue(audioWaveElement.activeSimulatedBumps_[0]!.maxVol > 0);
  });

  test('resize observer updates container width', async () => {
    audioWaveElement.style.position = 'absolute';
    audioWaveElement.style.display = 'block';

    audioWaveElement.style.setProperty('width', '100px', 'important');

    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();

    assertTrue(audioWaveElement.containerWidth_ > 0);

    audioWaveElement.style.setProperty('width', '500px', 'important');

    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await microtasksFinished();

    assertEquals(500, audioWaveElement.containerWidth_);
  });

  test('weightedAverage prioritizes recent values', () => {
    // Uniform.
    assertEquals(1, weightedAverage([1, 1, 1, 1], 4));

    // High at start.
    assertEquals(5, weightedAverage([10, 0, 0], 3));

    // High at end.
    const result = weightedAverage([0, 0, 10], 3);
    assertTrue(result > 1.6 && result < 1.7);
  });

  test('bezierEasing output and linear checks', () => {
    // 0->`timeProgress` returns 0; 1->`timeProgress` returns 1.
    assertEquals(0, bezierEasing(0.25, 0.75, 0));
    assertEquals(1, bezierEasing(0.25, 0.75, 1));

    // Linear Configuration: (0, 1) -
    // Return `timeProgress` (3rd argument) exactly.
    assertEquals(0.5, bezierEasing(0, 1, 0.5));
    assertEquals(0.25, bezierEasing(0, 1, 0.25));

    // Non-Linear (Ease-in logic):
    assertAlmostEquals(0.6423340207536106, bezierEasing(0.25, 0.5, 0.5));
  });

  test('simulated bump physics (mountain-like cosine wave)', () => {
    audioWaveElement.activeSimulatedBumps_ = [];
    audioWaveElement.frame_ = 0;

    audioWaveElement.activeSimulatedBumps_.push(
        {startTime: 0, duration: 100, maxVol: 1.0});

    audioWaveElement.frame_ = 0;
    let vol = audioWaveElement.getSimulatedAudioBumpsSum_();
    assertTrue(vol < 0.01, `Expected start vol ~0, got ${vol}`);

    audioWaveElement.frame_ = 50;
    vol = audioWaveElement.getSimulatedAudioBumpsSum_();
    assertTrue(vol > 0.9, `Expected peak vol ~1, got ${vol}`);

    audioWaveElement.frame_ = 100;
    vol = audioWaveElement.getSimulatedAudioBumpsSum_();
    assertTrue(vol < 0.01, `Expected end vol ~0, got ${vol}`);
  });

  test('simulated bumps are garbage collected', () => {
    audioWaveElement.activeSimulatedBumps_ = [];
    audioWaveElement.frame_ = 0;

    audioWaveElement.activeSimulatedBumps_.push(
        {startTime: 0, duration: 10, maxVol: 1.0});

    assertEquals(1, audioWaveElement.activeSimulatedBumps_.length);

    audioWaveElement.frame_ = 11;
    audioWaveElement.getSimulatedAudioBumpsSum_();

    assertEquals(0, audioWaveElement.activeSimulatedBumps_.length);
  });

  test('idle breathing animation oscillates deterministically', () => {
    const originalRandom = Math.random;
    Math.random = () => 0;

    const expectedAnimationValues: number[] = [
      0,
      0.0006992884163251478,
      0.0014140855087388926,
      0.0021319126771943923,
      0.0028400153060840912,
      0.003525653274483929,
      0.004176396018014396,
      0.004780415825534747,
      0.005326773085301104,
      0.005805687351648671,
      0.006208788380894756,
      0.006529341677917716,
      0.0067624435945191465,
      0.00690518161702331,
      0.00695675616164657,
      0.0069185609484931606,
      0.006794219833853846,
      0.006589578830079175,
      0.006312652916273636,
      0.005973528124655071,
      0.0055842202598438435,
      0.005158492455055943,
      0.004711634574217572,
      0.004260208217309461,
      0.0038217617638471642,
      0.0034145204837662813,
      0.00305705724519041,
      0.0027679497455101567,
      0.0025654304787648503,
      0.0024670358234581357,
      0.00248926068779301,
      0.0026472250832336864,
      0.0029543588138550604,
      0.003422110171859864,
      0.004059684124748735,
      0.004873814974705478,
      0.00704524291685502,
      0.008402174747765329,
      0.009934779910787776,
      0.011635503233952682,
      0.013085511895722548,
      0.014586219938238441,
      0.016119027659073384,
      0.01766531120047718,
      0.019206623420037792,
      0.020724888909408924,
      0.02220259118502904,
      0.023622950210470527,
      0.024970088562503222,
      0.026229184719828696,
      0.027386612132336417,
      0.02843006291718664,
      0.029348655223542717,
      0.03013302350787269,
      0.03077539116396087,
      0.03126962515373613,
      0.031611272484441004,
      0.03179757857237162,
      0.031827487721385946,
      0.03170162612375116,
      0.03170162612375116,
    ];

    try {
      audioWaveElement.onStartListen_();
      audioWaveElement.activeSimulatedBumps_ = [];
      audioWaveElement.frame_ = 0;

      audioWaveElement.updateVolume_();
      assertTrue(
          audioWaveElement.volumeHistory_ &&
          audioWaveElement.volumeHistory_.length > 0);
      const initialVol = audioWaveElement.volumeHistory_[0]!;

      assertEquals(0, initialVol, 'Idle volume should be 0');

      let hasChanged = false;
      let previousVol = initialVol;
      assertTrue(
          audioWaveElement.volumeHistory_ &&
          audioWaveElement.volumeHistory_.length > 0);

      // Simulate 1 second (60 frames).
      for (let i = 0; i < 60; i++) {
        audioWaveElement.frame_++;
        audioWaveElement.updateVolume_();

        const currentVol = audioWaveElement.volumeHistory_[0]!;
        assertTrue(!!currentVol);
        if (Math.abs(currentVol - previousVol) > 0.0001) {
          hasChanged = true;
        }
        assertTrue(hasChanged);
        assertAlmostEquals(expectedAnimationValues[i]!, currentVol, 0.1);
        previousVol = currentVol;
      }
    } finally {
      Math.random = originalRandom;
    }
  });

  test('volume history reflects ambient noise + bumps', () => {
    const originalRandom = Math.random;
    Math.random = () => 0;

    const expectedVolumeBumps: number[] = [
      // Rising phase:
      0.025171030268748326,
      0.09690558832126517,
      0.20823928653095783,
      0.34833151811861035,
      0.503525653274484,
      0.6586848932054882,
      0.7986730419717712,
      0.9098352702727748,
      0.9813339454992255,
      1.0062087883808948,

      // Falling phase:
      0.9820575998254946,
      0.9112709407819929,
      0.80079780776326,
      0.6614652533491203,
      0.5069185609484933,
      0.35228572264638025,
      0.21269695268384273,
      0.10180415572879992,
      0.03044526997707825,
      0.0055842202598438435,
    ];

    try {
      audioWaveElement.onStartListen_();
      audioWaveElement.activeSimulatedBumps_ = [];
      audioWaveElement.frame_ = 0;

      audioWaveElement.updateVolume_();

      assertTrue(
          audioWaveElement.volumeHistory_ &&
          audioWaveElement.volumeHistory_.length > 0);
      const quietLevel = audioWaveElement.volumeHistory_[0]!;

      // Bump of duration 20, max volume of 1
      audioWaveElement.makeSimulatedAudioBump_(
          0, 20, audioWaveElement.frame_, 0, 1.0);

      // Move halfway
      for (let i = 0; i < 10; i++) {
        audioWaveElement.frame_++;
        audioWaveElement.updateVolume_();
        const currentVol = audioWaveElement.volumeHistory_[0];
        assertTrue(!!currentVol);
        assertAlmostEquals(expectedVolumeBumps[i]!, currentVol, 0.005);
      }
      assertTrue(
          audioWaveElement.volumeHistory_ &&
          audioWaveElement.volumeHistory_.length > 0);
      const peakLevel = audioWaveElement.volumeHistory_[0]!;

      assertTrue(
          peakLevel > quietLevel + 0.5,
          `Volume should rise. Quiet: ${quietLevel}, Peak: ${peakLevel}`);

      const secondHalfStart = 10;
      for (let i = 0; i < 10; i++) {
        audioWaveElement.frame_++;
        audioWaveElement.updateVolume_();
        const currentVol = audioWaveElement.volumeHistory_[0];
        assertTrue(!!currentVol);
        assertAlmostEquals(
            expectedVolumeBumps[i + secondHalfStart]!, currentVol, 0.005);
      }

      assertTrue(
          audioWaveElement.volumeHistory_ &&
          audioWaveElement.volumeHistory_.length > 0);
      const endLevel = audioWaveElement.volumeHistory_[0]!;

      assertTrue(
          Math.abs(endLevel - quietLevel) < 0.1,
          `Volume should decay. End: ${endLevel}, Baseline: ${quietLevel}`);

    } finally {
      Math.random = originalRandom;
    }
  });
});
