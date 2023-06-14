// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, routesMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

const {Section} = routesMojom;
type PageName = keyof typeof Section;

suite('Page availability', () => {
  setup(() => {
    // Setup consistent load time data for each test so overrides do not carry
    // over between tests
    loadTimeData.resetForTesting({
      isGuest: false,
      isKerberosEnabled: false,
      allowPowerwash: false,
    });
  });

  /**
   * The availability of these pages depends on load time data
   */
  function runLoadTimeControlledTests() {
    interface LoadTimeControlledPage {
      pageName: PageName;
      loadTimeId: string;
    }

    const loadTimeControlled: LoadTimeControlledPage[] = [
      {pageName: 'kKerberos', loadTimeId: 'isKerberosEnabled'},
      {pageName: 'kReset', loadTimeId: 'allowPowerwash'},
    ];
    loadTimeControlled.forEach(({pageName, loadTimeId}) => {
      test(`${pageName} page is available when ${loadTimeId}=true`, () => {
        loadTimeData.overrideValues({[loadTimeId]: true});
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[Section[pageName]]);
      });

      test(`${pageName} page is unavailable when ${loadTimeId}=false`, () => {
        loadTimeData.overrideValues({[loadTimeId]: false});
        const pageAvailability = createPageAvailabilityForTesting();
        assertFalse(!!pageAvailability[Section[pageName]]);
      });
    });
  }

  suite('When signed in as user', () => {
    const alwaysAvailable: PageName[] = [
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
    alwaysAvailable.forEach((pageName) => {
      test(`${pageName} page should always be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[Section[pageName]]);
      });
    });

    runLoadTimeControlledTests();
  });

  suite('When in guest mode', () => {
    setup(() => {
      loadTimeData.overrideValues({isGuest: true});
    });

    const alwaysAvailable: PageName[] = [
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
    alwaysAvailable.forEach((pageName) => {
      test(`${pageName} page should always be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[Section[pageName]]);
      });
    });

    const neverAvailable: PageName[] = [
      'kFiles',
      'kMultiDevice',
      'kPeople',
      'kPersonalization',
    ];
    neverAvailable.forEach((pageName) => {
      test(`${pageName} page should never be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertFalse(!!pageAvailability[Section[pageName]]);
      });
    });

    runLoadTimeControlledTests();
  });
});
