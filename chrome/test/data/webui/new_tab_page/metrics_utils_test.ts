// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration, recordOccurence, recordPerdecage} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';

suite('NewTabPageMetricsUtilsTest', () => {
  let metrics: MetricsTracker;

  setup(() => {
    metrics = fakeMetricsPrivate();
  });

  test('recordDuration', () => {
    recordDuration('foo.metric', 123.0);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 123));
  });

  test('recordLoadDuration', () => {
    loadTimeData.overrideValues({
      navigationStartTime: 5.0,
    });
    recordLoadDuration('foo.metric', 123.0);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 118));
  });

  test('recordPerdecage', () => {
    recordPerdecage('foo.metric', 5);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 5));
  });

  test('recordOccurence', () => {
    recordOccurence('foo.metric');
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 1));
  });
});
