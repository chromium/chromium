// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, OsPageAvailability} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
      pageName: keyof OsPageAvailability;
      loadTimeId: string;
    }

    const loadTimeControlled: LoadTimeControlledPage[] = [
      {pageName: 'kerberos', loadTimeId: 'isKerberosEnabled'},
      {pageName: 'osReset', loadTimeId: 'allowPowerwash'},
    ];
    loadTimeControlled.forEach(({pageName, loadTimeId}) => {
      test(`${pageName} page is available when ${loadTimeId}=true`, () => {
        loadTimeData.overrideValues({[loadTimeId]: true});
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[pageName]);
      });

      test(`${pageName} page is unavailable when ${loadTimeId}=false`, () => {
        loadTimeData.overrideValues({[loadTimeId]: false});
        const pageAvailability = createPageAvailabilityForTesting();
        assertFalse(!!pageAvailability[pageName]);
      });
    });
  }

  suite('When signed in as user', () => {
    const alwaysAvailable: Array<keyof OsPageAvailability> = [
      'apps',
      'bluetooth',
      'crostini',
      'dateTime',
      'device',
      'files',
      'internet',
      'multidevice',
      'osAccessibility',
      'osLanguages',
      'osPeople',
      'osPrinting',
      'osPrivacy',
      'osSearch',
      'personalization',
    ];
    alwaysAvailable.forEach((pageName) => {
      test(`${pageName} page should always be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[pageName]);
      });
    });

    runLoadTimeControlledTests();
  });

  suite('When in guest mode', () => {
    setup(() => {
      loadTimeData.overrideValues({isGuest: true});
    });

    const alwaysAvailable: Array<keyof OsPageAvailability> = [
      'apps',
      'bluetooth',
      'crostini',
      'dateTime',
      'device',
      'internet',
      'osAccessibility',
      'osLanguages',
      'osPrinting',
      'osPrivacy',
      'osSearch',
    ];
    alwaysAvailable.forEach((pageName) => {
      test(`${pageName} page should always be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertTrue(!!pageAvailability[pageName]);
      });
    });

    const neverAvailable: Array<keyof OsPageAvailability> = [
      'files',
      'multidevice',
      'osPeople',
      'personalization',
    ];
    neverAvailable.forEach((pageName) => {
      test(`${pageName} page should never be available`, () => {
        const pageAvailability = createPageAvailabilityForTesting();
        assertFalse(!!pageAvailability[pageName]);
      });
    });

    runLoadTimeControlledTests();
  });
});
