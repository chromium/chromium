// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip.top-chrome/alert_indicators.js';

import type {AlertIndicatorsElement} from 'chrome://tab-strip.top-chrome/alert_indicators.js';
import {TabAlertState} from 'chrome://tab-strip.top-chrome/tabs.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('AlertIndicators', () => {
  let alertIndicatorsElement: AlertIndicatorsElement;

  function getAlertIndicators() {
    return alertIndicatorsElement.shadowRoot!.querySelectorAll(
        'tabstrip-alert-indicator');
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    alertIndicatorsElement =
        document.createElement('tabstrip-alert-indicators');
    document.body.appendChild(alertIndicatorsElement);
  });

  test('creates an alert indicator for each alert state', () => {
    alertIndicatorsElement.updateAlertStates([
      TabAlertState.kPipPlaying,
      TabAlertState.kVrPresentingInHeadset,
    ]);
    const createdAlertIndicators = getAlertIndicators();
    assertEquals(createdAlertIndicators.length, 2);
  });

  test(
      're-uses a shared alert indicator when necessary and prioritizes ' +
          'the earlier alert state in the list',
      async () => {
        async function assertSharedIndicator(
            prioritizedState: TabAlertState, ignoredState: TabAlertState) {
          await alertIndicatorsElement.updateAlertStates([ignoredState]);
          let alertIndicators = getAlertIndicators();
          const sharedIndicator = alertIndicators[0]!;
          assertEquals(alertIndicators.length, 1);
          assertEquals(sharedIndicator.alertState, ignoredState);

          await alertIndicatorsElement.updateAlertStates(
              [prioritizedState, ignoredState]);
          alertIndicators = getAlertIndicators();
          assertEquals(alertIndicators.length, 1);
          assertEquals(alertIndicators[0]!, sharedIndicator);
          assertEquals(sharedIndicator.alertState, prioritizedState);
        }

        await assertSharedIndicator(
            TabAlertState.kAudioMuting, TabAlertState.kAudioPlaying);
        await assertSharedIndicator(
            TabAlertState.kMediaRecording, TabAlertState.kDesktopCapturing);
      });

  test('removes alert indicators when needed', async () => {
    await alertIndicatorsElement.updateAlertStates([
      TabAlertState.kPipPlaying,
      TabAlertState.kVrPresentingInHeadset,
    ]);

    await alertIndicatorsElement.updateAlertStates([TabAlertState.kPipPlaying]);
    const alertIndicators = getAlertIndicators();
    assertEquals(alertIndicators.length, 1);
    assertEquals(alertIndicators[0]!.alertState, TabAlertState.kPipPlaying);
  });

  test(
      'updating alert states returns a promise with a returned value ' +
          'representing the number of alert indicators',
      async () => {
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.kPipPlaying,
              TabAlertState.kVrPresentingInHeadset,
            ]),
            2);
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.kPipPlaying,
            ]),
            1);
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.kAudioMuting,
              TabAlertState.kAudioPlaying,
            ]),
            1);
      });

  test(
      'updating alert states multiple times in succession resolves with the ' +
          'last update',
      async () => {
        alertIndicatorsElement.updateAlertStates([
          TabAlertState.kPipPlaying,
          TabAlertState.kVrPresentingInHeadset,
        ]);
        alertIndicatorsElement.updateAlertStates([]);

        await alertIndicatorsElement.updateAlertStates([
          TabAlertState.kPipPlaying,
          TabAlertState.kAudioPlaying,
        ]);

        const alertIndicators = getAlertIndicators();
        assertEquals(alertIndicators.length, 2);
        assertEquals(alertIndicators[0]!.alertState, TabAlertState.kPipPlaying);
        assertEquals(
            alertIndicators[1]!.alertState, TabAlertState.kAudioPlaying);
      });
});
