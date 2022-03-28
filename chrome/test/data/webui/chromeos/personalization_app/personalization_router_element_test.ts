// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Paths, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {initElement} from './personalization_app_test_utils.js';

export function PersonalizationRouterTest() {
  test('redirects if hub feature off on root page', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': false});
    const reloadCalledPromise = new Promise<void>((resolve) => {
      PersonalizationRouter.reloadAtWallpaper = resolve;
    });
    initElement(PersonalizationRouter);
    await reloadCalledPromise;
  });

  test('will show ambient subpage if allowed', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': true});
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    const routerElement = initElement(PersonalizationRouter);
    PersonalizationRouter.instance().goToRoute(Paths.Ambient);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertFalse(!!mainElement);

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertTrue(!!ambientSubpage);
  });

  test('will not show ambient subpage if disallowed', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': true});
    loadTimeData.overrideValues({'isAmbientModeAllowed': false});
    const routerElement = initElement(PersonalizationRouter);
    PersonalizationRouter.instance().goToRoute(Paths.Ambient);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertFalse(!!ambientSubpage);
  });
}
