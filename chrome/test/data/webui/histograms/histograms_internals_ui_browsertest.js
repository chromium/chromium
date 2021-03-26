// Copyright 2020 The Chromium Authors. All rights reserved.
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
