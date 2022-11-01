// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Histograms WebUI.
 */

GEN('#include "base/metrics/histogram.h"');
GEN('#include "content/public/test/browser_test.h"');

function HistogramsInternalsUIBrowserTest() {}

HistogramsInternalsUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://histograms',

  isAsync: true,

  testGenPreamble: function() {
    GEN('base::HistogramBase* histogram =');
    GEN('    base::LinearHistogram::FactoryGet("HTMLOut", /*minimum=*/1, /*maximum=*/10,');
    GEN('                                /*bucket_count=*/5, base::HistogramBase::kNoFlags);');
    GEN('histogram->AddCount(/*sample=*/4, /*value=*/5);');
    GEN('base::HistogramBase* histogram1 =');
    GEN('    base::LinearHistogram::FactoryGet("HTMLOut1", /*minimum=*/1, /*maximum=*/20,');
    GEN('                                /*bucket_count=*/4, base::HistogramBase::kNoFlags);');
    GEN('histogram1->AddCount(/*sample=*/10, /*value=*/2);');
    GEN('histogram1->AddCount(/*sample=*/15, /*value=*/4);');
  },

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

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

TEST_F('HistogramsInternalsUIBrowserTest', 'RefreshHistograms', function() {
  test('check refresh button replaces existing histograms', async function() {
    document.querySelector('#refresh').click();
    await histogramsUpdated();
    const histogramHeader = 'Histogram: HTMLOut recorded 5 samples';
    assertEquals(
        document.body.textContent.indexOf(histogramHeader),
        document.body.textContent.lastIndexOf(histogramHeader),
        'refresh should replace existing histograms');
  });

  mocha.run();
});

TEST_F('HistogramsInternalsUIBrowserTest', 'NoDummyHistograms', function() {
  test('check no dummy histogram is present', async function() {
    document.querySelector('#refresh').click();
    await histogramsUpdated();
    document.querySelectorAll('.histogram-header-text').forEach(header => {
      assertNotEquals(header.textContent, '');
    });
  });

  mocha.run();
});

TEST_F('HistogramsInternalsUIBrowserTest', 'DownloadHistograms', function() {
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

  test('check downloaded content is in expected format', function() {
    assertNotEquals(
        document.generateHistogramsForTest().indexOf(expectedContent), -1);
  });

  mocha.run();
});

TEST_F('HistogramsInternalsUIBrowserTest', 'StopMonitoring', function() {
  test('check page stops updating', async function() {
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

  mocha.run();
});

TEST_F('HistogramsInternalsUIBrowserTest', 'SubprocessCheckbox', function() {
  test('check refresh histograms from clicking on checkbox', async function() {
    await histogramsAreShown();
    const subprocessCheckbox = document.querySelector('#subprocess_checkbox');
    assertFalse(subprocessCheckbox.disabled);
    assertFalse(subprocessCheckbox.hasAttribute('title'));
    subprocessCheckbox.click();
    await histogramsUpdated();
  });

  mocha.run();
});

TEST_F(
    'HistogramsInternalsUIBrowserTest', 'SubprocessCheckboxInMonitoringMode',
    function() {
      test(
          'check refresh histograms from clicking on checkbox',
          async function() {
            // Enable monitoring mode.
            document.querySelector('#enable_monitoring').click();
            await histogramsAreShown();
            const subprocessCheckbox =
                document.querySelector('#subprocess_checkbox');
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

      mocha.run();
    });
