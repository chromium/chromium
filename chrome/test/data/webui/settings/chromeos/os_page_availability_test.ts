// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, createRouterForTesting, OsPageAvailability, Router, routesMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

const {Section} = routesMojom;
type SectionName = keyof typeof Section;

function initializePageAvailability(): OsPageAvailability {
  const testRouter = createRouterForTesting();
  Router.resetInstanceForTesting(testRouter);
  return createPageAvailabilityForTesting();
}

suite('Page availability', () => {
  /**
   * The availability of these pages depends on load time data
   */
  function runLoadTimeControlledTests() {
    interface LoadTimeControlledData {
      sectionName: SectionName;
      loadTimeId: string;
    }

    const loadTimeControlled: LoadTimeControlledData[] = [
      {sectionName: 'kKerberos', loadTimeId: 'isKerberosEnabled'},
      {sectionName: 'kReset', loadTimeId: 'allowPowerwash'},
    ];
    loadTimeControlled.forEach(({sectionName, loadTimeId}) => {
      test(
          `${sectionName} page is available when ${loadTimeId} is true`, () => {
            loadTimeData.overrideValues({[loadTimeId]: true});
            const pageAvailability = initializePageAvailability();
            assertTrue(pageAvailability[Section[sectionName]]);
          });

      test(
          `${sectionName} page is unavailable when ${loadTimeId} is false`,
          () => {
            loadTimeData.overrideValues({[loadTimeId]: false});
            const pageAvailability = initializePageAvailability();
            assertFalse(pageAvailability[Section[sectionName]]);
          });
    });
  }

  suite('When signed in as user', () => {
    setup(() => {
      loadTimeData.overrideValues({isGuest: false});
    });

    const availablePages: SectionName[] = [
      'kAccessibility',
      'kApps',
      'kBluetooth',
      'kCrostini',
      'kDateAndTime',
      'kDevice',
      'kFiles',
      'kLanguagesAndInput',
      'kMultiDevice',
      'kNetwork',
      'kPeople',
      'kPersonalization',
      'kPrinting',
      'kPrivacyAndSecurity',
      'kSearchAndAssistant',
    ];
    availablePages.forEach((sectionName) => {
      test(`${sectionName} page should always be available`, () => {
        const pageAvailability = initializePageAvailability();
        assertTrue(pageAvailability[Section[sectionName]]);
      });
    });

    runLoadTimeControlledTests();
  });

  suite('When in guest mode', () => {
    setup(() => {
      loadTimeData.overrideValues({isGuest: true});
    });

    const availablePages: SectionName[] = [
      'kAccessibility',
      'kApps',
      'kBluetooth',
      'kCrostini',
      'kDateAndTime',
      'kDevice',
      'kLanguagesAndInput',
      'kNetwork',
      'kPrinting',
      'kPrivacyAndSecurity',
      'kSearchAndAssistant',
    ];
    availablePages.forEach((sectionName) => {
      test(`${sectionName} page should always be available`, () => {
        const pageAvailability = initializePageAvailability();
        assertTrue(pageAvailability[Section[sectionName]]);
      });
    });

    const unavailablePages: SectionName[] = [
      'kFiles',
      'kMultiDevice',
      'kPeople',
      'kPersonalization',
    ];
    unavailablePages.forEach((sectionName) => {
      test(`${sectionName} page should never be available`, () => {
        const pageAvailability = initializePageAvailability();
        assertFalse(pageAvailability[Section[sectionName]]);
      });
    });

    runLoadTimeControlledTests();
  });
});
