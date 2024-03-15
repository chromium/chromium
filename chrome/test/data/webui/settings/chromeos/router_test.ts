// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the Router singleton.
 */

import 'chrome://os-settings/os_settings.js';

import {createRouterForTesting, Router, routesMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Router', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let router: Router;

  setup(() => {
    router = createRouterForTesting();
  });

  if (isRevampWayfindingEnabled) {
    suite('Redirection', () => {
      const redirectPairs = [
        [
          routesMojom.MY_ACCOUNTS_SUBPAGE_PATH,
          routesMojom.PEOPLE_SECTION_PATH,
        ],
        [
          routesMojom.DATE_AND_TIME_SECTION_PATH,
          routesMojom.SYSTEM_PREFERENCES_SECTION_PATH,
        ],
        [
          routesMojom.FILES_SECTION_PATH,
          routesMojom.SYSTEM_PREFERENCES_SECTION_PATH,
        ],
        [
          routesMojom.BLUETOOTH_SECTION_PATH,
          routesMojom.BLUETOOTH_DEVICES_SUBPAGE_PATH,
        ],
      ];
      redirectPairs.forEach(([path, redirectPath]) => {
        test(
            `"${path}" should redirect to route with "${redirectPath}" path`,
            () => {
              const route = router.getRouteForPath(`/${path}`);
              assertTrue(!!route);
              assertEquals(`/${redirectPath}`, route.path);
            });
      });
    });
  }
});
