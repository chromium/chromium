// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsPersonalizationOptionsElement} from 'chrome://os-settings/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

function buildTestElement(deprecateSyncMetric: boolean):
    OsSettingsPersonalizationOptionsElement {
  loadTimeData.overrideValues({
    osDeprecateSyncMetricsToggle: deprecateSyncMetric,
  });

  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const testElement =
      document.createElement('os-settings-personalization-options');
  testElement.prefs = {};
  document.body.appendChild(testElement);
  flush();
  return testElement;
}

suite('PersonalizationOptionsTestsDeprecateSyncMetricsOn', () => {
  let testElement: OsSettingsPersonalizationOptionsElement;

  setup(() => {
    testElement = buildTestElement(/*deprecateSyncMetric=*/ true);
  });

  teardown(() => {
    testElement.remove();
  });

  test('Metrics toggle do not show with deprecate sync metrics on', () => {
    assertFalse(isChildVisible(testElement, '#metricsReportingControl'));
  });
});

suite('PersonalizationOptionsTestsDeprecateSyncMetricsOff', () => {
  let testElement: OsSettingsPersonalizationOptionsElement;

  setup(() => {
    testElement = buildTestElement(/*deprecateSyncMetric=*/ false);
  });

  teardown(() => {
    testElement.remove();
  });

  test('Metrics toggle shows with deprecate sync metrics off', () => {
    assertTrue(isChildVisible(testElement, '#metricsReportingControl'));
  });
});
