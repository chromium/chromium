// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordBoolean, recordDuration, recordEnumeration, recordLinearValue, recordLoadDuration, recordLogValue, recordOccurrence, recordPerdecage, recordSmallCount, recordSparseValueWithPersistentHash} from 'chrome://new-tab-page/new_tab_page.js';
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

  test('recordOccurrence', () => {
    recordOccurrence('foo.metric');
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 1));
  });

  test('recordSmallCount', () => {
    recordSmallCount('foo.metric', 5);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 5));
  });

  test('recordEnumeration', () => {
    recordEnumeration('foo.metric', 5, 10);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 5));
  });

  test('recordSparseValueWithPersistentHash', () => {
    recordSparseValueWithPersistentHash('foo.metric', 'bar');
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 'bar'));
  });

  test('recordBoolean', () => {
    recordBoolean('foo.metric', true);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', true));
  });

  test('recordLogValue', () => {
    recordLogValue(
        'foo.metric',
        /*min=*/ 1,
        /*max=*/ 10,
        /*buckets=*/ 11,
        /*value=*/ 5);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 5));
  });

  test('recordLinearValue', () => {
    recordLinearValue(
        'foo.metric',
        /*min=*/ 1,
        /*max=*/ 10,
        /*buckets=*/ 11,
        /*value=*/ 5);
    assertEquals(1, metrics.count('foo.metric'));
    assertEquals(1, metrics.count('foo.metric', 5));
  });
});
