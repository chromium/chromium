// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration, recordPerdecage, recordOccurence} from 'chrome://new-tab-page/new_tab_page.js';
import {fakeMetricsPrivate, MetricsTracker} from './metrics_test_support.js';
import {assertEquals} from '../chai_assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

suite('NewTabPageMetricsUtilsTest', () => {
  /** @type {MetricsTracker} */
  let metrics;

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
