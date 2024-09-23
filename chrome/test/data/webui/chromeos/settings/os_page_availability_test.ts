// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page availability.
 *
 * - This suite is run with the OsSettingsRevampWayfinding feature flag both
 *   enabled and disabled.
 */

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, createRouterForTesting, OsPageAvailability, Router, routesMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {SECTION_EXPECTATIONS} from './os_settings_ui/page_availability_test_helpers.js';

const {Section} = routesMojom;

function initializePageAvailability(): OsPageAvailability {
  const testRouter = createRouterForTesting();
  Router.resetInstanceForTesting(testRouter);
  return createPageAvailabilityForTesting();
}

suite('Page availability', () => {
  const isRevampEnabled = loadTimeData.getBoolean('isRevampWayfindingEnabled');

  suite('For normal user', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isGuest: false,           // Simulate normal user
        isKerberosEnabled: true,  // Simulate kerberos enabled
        allowPowerwash: true,     // Simulate powerwash allowed
      });
    });

    for (const {
           name,
           availableBeforeRevamp,
           availableAfterRevamp,
         } of SECTION_EXPECTATIONS) {
      test(`${name} page availability`, () => {
        const shouldExpectAvailable =
            (isRevampEnabled && availableAfterRevamp) ||
            (!isRevampEnabled && availableBeforeRevamp);

        const pageAvailability = initializePageAvailability();
        const isAvailable = pageAvailability[Section[name]];
        assertEquals(shouldExpectAvailable, isAvailable);
      });
    }

    test('kKerberos page is not available when isKerberosEnabled=false', () => {
      loadTimeData.overrideValues({isKerberosEnabled: false});
      const pageAvailability = initializePageAvailability();
      const isAvailable = pageAvailability[Section.kKerberos];
      assertFalse(isAvailable);
    });

    test('kReset page is not available when allowPowerwash=false', () => {
      loadTimeData.overrideValues({allowPowerwash: false});
      const pageAvailability = initializePageAvailability();
      const isAvailable = pageAvailability[Section.kReset];
      assertFalse(isAvailable);
    });
  });

  suite('For guest user', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isGuest: true,            // Simulate guest mode
        isKerberosEnabled: true,  // Simulate kerberos enabled
        allowPowerwash: false,    // Powerwash is never enabled in guest mode
      });
    });

    for (const {
           name,
           availableBeforeRevamp,
           availableAfterRevamp,
           availableForGuest,
         } of SECTION_EXPECTATIONS) {
      test(`${name} page availability`, () => {
        const shouldExpectAvailable = availableForGuest &&
            ((isRevampEnabled && availableAfterRevamp) ||
             (!isRevampEnabled && availableBeforeRevamp));

        const pageAvailability = initializePageAvailability();
        const isAvailable = pageAvailability[Section[name]];
        assertEquals(shouldExpectAvailable, isAvailable);
      });
    }

    test('kKerberos page is not available when isKerberosEnabled=false', () => {
      loadTimeData.overrideValues({isKerberosEnabled: false});
      const pageAvailability = initializePageAvailability();
      const isAvailable = pageAvailability[Section.kKerberos];
      assertFalse(isAvailable);
    });

    test('kReset page is never available in guest mode', () => {
      const pageAvailability = initializePageAvailability();
      const isAvailable = pageAvailability[Section.kReset];
      assertFalse(isAvailable);
    });
  });
});
