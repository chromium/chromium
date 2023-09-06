// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * @returns {Promise} a Promise that will resolve on the histograms tag
 * receiving a 'histograms-updated-for-test' event.
 */
async function histogramsUpdated() {
  return new Promise((resolve) => {
    document.querySelector('#histograms')
        .addEventListener('histograms-updated-for-test', resolve);
  });
}

/**
 * @returns {Promise} a Promise that will resolve when there is at least one
 * histogram shown on the page.
 */
async function histogramsAreShown() {
  return new Promise((resolve) => {
    const checkHistograms = () => {
      if (document.querySelector('#histograms').childElementCount > 0) {
        resolve();
      } else {
        setTimeout(checkHistograms, 250);
      }
    };
    checkHistograms();
  });
}

suite('HistogramsInternals', () => {
  test('RefreshHistograms', async function() {
    document.querySelector('#refresh').click();
    await histogramsUpdated();
    const histogramHeader = 'Histogram: HTMLOut recorded 5 samples';
    const indexOfHeader = document.body.textContent.indexOf(histogramHeader);
    assertNotEquals(-1, indexOfHeader);
    assertEquals(
        indexOfHeader, document.body.textContent.lastIndexOf(histogramHeader),
        'refresh should replace existing histograms');
  });

  test('NoDummyHistograms', async function() {
    document.querySelector('#refresh').click();
    await histogramsUpdated();
    document.querySelectorAll('.histogram-header-text').forEach(header => {
      assertNotEquals(header.textContent, '');
    });
  });

  test('DownloadHistograms', function() {
    // We can assume these two histograms appear next to each other since
    // the histograms are sorted by name.
    const expectedContent =
        '- Histogram: HTMLOut recorded 5 samples, mean = 4.0 (flags = 0x40) [#]\n\n' +
        '0  ... \n' +
        '4  -----O                                                                    (5 = 100.0%) {0.0%}\n' +
        '7  ... \n\n\n' +
        '- Histogram: HTMLOut1 recorded 6 samples, mean = 13.3 (flags = 0x40) [#]\n\n' +
        '0   O                                                                         (0 = 0.0%)\n' +
        '1   --O                                                                       (2 = 33.3%) {0.0%}\n' +
        '11  ----O                                                                     (4 = 66.7%) {33.3%}\n' +
        '20  O                                                                         (0 = 0.0%) {100.0%}';

    assertNotEquals(
        document.generateHistogramsForTest().indexOf(expectedContent), -1);
  });

  test('StopMonitoring', async function() {
    document.querySelector('#enable_monitoring').click();
    // Wait until histograms are updated in monitoring mode.
    await histogramsUpdated();

    const stopButton = document.querySelector('#stop');
    assertFalse(document.monitoringStopped());
    assertEquals(stopButton.textContent, 'Stop');
    assertFalse(stopButton.disabled);

    stopButton.click();
    assertTrue(document.monitoringStopped());
    assertEquals(stopButton.textContent, 'Stopped');
    assertTrue(stopButton.disabled);
  });

  test('SubprocessCheckbox', async function() {
    await histogramsAreShown();
    const subprocessCheckbox = document.querySelector('#subprocess_checkbox');
    assertFalse(subprocessCheckbox.disabled);
    assertFalse(subprocessCheckbox.hasAttribute('title'));
    subprocessCheckbox.click();
    await histogramsUpdated();
  });

  test('SubprocessCheckboxInMonitoringMode', async function() {
    // Enable monitoring mode.
    document.querySelector('#enable_monitoring').click();
    await histogramsAreShown();
    const subprocessCheckbox = document.querySelector('#subprocess_checkbox');
    // Subprocess checkbox will be disabled when monitoring mode is on.
    assertTrue(subprocessCheckbox.disabled);
    assertTrue(subprocessCheckbox.hasAttribute('title'));

    // Stop monitoring mode.
    document.querySelector('#stop').click();
    assertTrue(subprocessCheckbox.disabled);

    // Exit monitoring mode.
    document.querySelector('#disable_monitoring').click();
    await histogramsAreShown();
    // Subprocess checkbox should be enabled again.
    assertFalse(subprocessCheckbox.disabled);
  });
});
