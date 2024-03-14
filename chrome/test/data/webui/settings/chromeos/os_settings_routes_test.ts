// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the Route class and collection of OsSettingsRoutes.
 */

import 'chrome://os-settings/os_settings.js';

import {createRoutesForTesting, OsSettingsRoutes} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertNull} from 'chrome://webui-test/chai_assert.js';

suite('Route', () => {
  let testRoutes: OsSettingsRoutes;

  setup(() => {
    testRoutes = createRoutesForTesting();
  });

  suite('getSectionAncestor()', () => {
    test('returns the top-level ancestor route', () => {
      const sectionAncestor = testRoutes.BLUETOOTH_DEVICES.getSectionAncestor();
      assertEquals(testRoutes.BLUETOOTH, sectionAncestor);
    });

    test('returns null if the route does not belong to a section', () => {
      const sectionAncestor = testRoutes.BASIC.getSectionAncestor();
      assertNull(sectionAncestor);
    });
  });
});
