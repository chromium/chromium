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

TEST_F('HistogramsInternalsUIBrowserTest', 'RefreshHistograms', function() {
  test(
      'check refresh button replaces existing histograms', function() {
        const whenRefreshed = new Promise((resolve, reject) => {
          document.querySelector('#histograms')
              .addEventListener('histograms-updated-for-test', resolve);
        });
        document.querySelector('#refresh').click();
        return whenRefreshed.then(() => {
          const histogramHeader = 'Histogram: HTMLOut recorded 5 samples';
          assertEquals(
              document.body.textContent.indexOf(histogramHeader),
              document.body.textContent.lastIndexOf(histogramHeader),
              'refresh should replace existing histograms');
        });
      });

  mocha.run();
});

TEST_F('HistogramsInternalsUIBrowserTest', 'NoDummyHistograms', function() {
  test(
      'check no dummy histogram is present', function() {
        const whenRefreshed = new Promise((resolve, reject) => {
          document.querySelector('#histograms')
              .addEventListener('histograms-updated-for-test', resolve);
        });
        document.querySelector('#refresh').click();
        return whenRefreshed.then(() => {
          document.querySelectorAll('.histogram-header-text')
              .forEach(header => {
                assertNotEquals(header.textContent, '');
              });
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
    // Make sure page is loaded after switching to monitoring mode.
    const loaded = new Promise((resolve, reject) => {
      document.querySelector('#histograms')
          .addEventListener('histograms-updated-for-test', resolve);
    });
    document.querySelector('#enable_monitoring').click();
    await loaded;
    const stopButton = document.querySelector('#stop');
    assertEquals(document.monitoringStopped(), false);
    assertEquals(stopButton.textContent, 'Stop');
    assertEquals(stopButton.disabled, false);
    stopButton.click();
    assertEquals(document.monitoringStopped(), true);
    assertEquals(stopButton.textContent, 'Stopped');
    assertEquals(stopButton.disabled, true);
  });

  mocha.run();
});
