// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/alert_indicators.js';

import {TabAlertState} from 'chrome://tab-strip/tabs_api_proxy.js';

suite('AlertIndicators', () => {
  let alertIndicatorsElement;

  const getAlertIndicators = () => {
    return alertIndicatorsElement.shadowRoot.querySelectorAll(
        'tabstrip-alert-indicator');
  };

  setup(() => {
    document.body.innerHTML = '';

    alertIndicatorsElement =
        document.createElement('tabstrip-alert-indicators');
    alertIndicatorsElement.onAlertIndicatorCountChange = () => {};
    document.body.appendChild(alertIndicatorsElement);
  });

  test('creates an alert indicator for each alert state', () => {
    alertIndicatorsElement.updateAlertStates([
      TabAlertState.PIP_PLAYING,
      TabAlertState.VR_PRESENTING_IN_HEADSET,
    ]);
    const createdAlertIndicators = getAlertIndicators();
    assertEquals(createdAlertIndicators.length, 2);
  });

  test(
      're-uses a shared alert indicator when necessary and prioritizes ' +
          'the earlier alert state in the list',
      async () => {
        async function assertSharedIndicator(prioritizedState, ignoredState) {
          await alertIndicatorsElement.updateAlertStates([ignoredState]);
          let alertIndicators = getAlertIndicators();
          const sharedIndicator = alertIndicators[0];
          assertEquals(alertIndicators.length, 1);
          assertEquals(sharedIndicator.alertState, ignoredState);

          await alertIndicatorsElement.updateAlertStates(
              [prioritizedState, ignoredState]);
          alertIndicators = getAlertIndicators();
          assertEquals(alertIndicators.length, 1);
          assertEquals(alertIndicators[0], sharedIndicator);
          assertEquals(sharedIndicator.alertState, prioritizedState);
        }

        await assertSharedIndicator(
            TabAlertState.AUDIO_MUTING, TabAlertState.AUDIO_PLAYING);
        await assertSharedIndicator(
            TabAlertState.MEDIA_RECORDING, TabAlertState.DESKTOP_CAPTURING);
      });

  test('removes alert indicators when needed', async () => {
    await alertIndicatorsElement.updateAlertStates([
      TabAlertState.PIP_PLAYING,
      TabAlertState.VR_PRESENTING_IN_HEADSET,
    ]);

    await alertIndicatorsElement.updateAlertStates([TabAlertState.PIP_PLAYING]);
    const alertIndicators = getAlertIndicators();
    assertEquals(alertIndicators.length, 1);
    assertEquals(alertIndicators[0].alertState, TabAlertState.PIP_PLAYING);
  });

  test(
      'updating alert states returns a promise with a returned value ' +
          'representing the number of alert indicators',
      async () => {
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.PIP_PLAYING,
              TabAlertState.VR_PRESENTING_IN_HEADSET,
            ]),
            2);
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.PIP_PLAYING,
            ]),
            1);
        assertEquals(
            await alertIndicatorsElement.updateAlertStates([
              TabAlertState.AUDIO_MUTING,
              TabAlertState.AUDIO_PLAYING,
            ]),
            1);
      });

  test(
      'updating alert states multiple times in succession resolves with the ' +
          'last update',
      async () => {
        alertIndicatorsElement.updateAlertStates([
          TabAlertState.PIP_PLAYING,
          TabAlertState.VR_PRESENTING_IN_HEADSET,
        ]);
        alertIndicatorsElement.updateAlertStates([]);

        await alertIndicatorsElement.updateAlertStates([
          TabAlertState.PIP_PLAYING,
          TabAlertState.AUDIO_PLAYING,
        ]);

        const alertIndicators = getAlertIndicators();
        assertEquals(alertIndicators.length, 2);
        assertEquals(alertIndicators[0].alertState, TabAlertState.PIP_PLAYING);
        assertEquals(
            alertIndicators[1].alertState, TabAlertState.AUDIO_PLAYING);
      });
});
