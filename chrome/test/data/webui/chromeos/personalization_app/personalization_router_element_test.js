// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationRouter} from 'chrome://personalization/trusted/personalization_router_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {initElement} from './personalization_app_test_utils.js';

export function PersonalizationRouterTest() {
  /** @type {?HTMLElement} */
  let personalizationRouterElement = null;

  teardown(async () => {
    personalizationRouterElement = null;
  });

  test('redirects if hub feature off on root page', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': false});
    const reloadCalledPromise = new Promise((resolve) => {
      PersonalizationRouter.reloadAtWallpaper = resolve;
    });
    initElement(PersonalizationRouter.is);
    await reloadCalledPromise;
  });
}
