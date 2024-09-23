// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsPersonalizationOptionsElement} from 'chrome://os-settings/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

function buildTestElement(): OsSettingsPersonalizationOptionsElement {
  clearBody();
  const testElement =
      document.createElement('os-settings-personalization-options');
  testElement.prefs = {};
  document.body.appendChild(testElement);
  flush();
  return testElement;
}

suite('PersonalizationOptionsTests', () => {
  let testElement: OsSettingsPersonalizationOptionsElement;

  setup(() => {
    testElement = buildTestElement();
  });

  teardown(() => {
    testElement.remove();
  });

  test('Metrics toggle do not show', () => {
    assertFalse(isChildVisible(testElement, '#metricsReportingControl'));
  });
});
