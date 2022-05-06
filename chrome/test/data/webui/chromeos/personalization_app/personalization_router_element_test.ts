// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {Paths, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';

suite('PersonalizationRouterTest', function() {
  setup(() => {
    baseSetup();
  });

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
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertEquals(getComputedStyle(mainElement).display, 'none');

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertTrue(!!ambientSubpage);
    assertNotEquals(getComputedStyle(ambientSubpage).display, 'none');
  });

  test('will not show ambient subpage if disallowed', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': true});
    loadTimeData.overrideValues({'isAmbientModeAllowed': false});
    const routerElement = initElement(PersonalizationRouter);
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertNotEquals(getComputedStyle(mainElement).display, 'none');

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertFalse(!!ambientSubpage);
  });

  test('returns to root page when wrong path is keyed in', async () => {
    loadTimeData.overrideValues({'isPersonalizationHubEnabled': true});
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    const routerElement = initElement(
        PersonalizationRouter, {path: '/wrongpath', queryParams: {}});
    await waitAfterNextRender(routerElement);

    // Due to the wrong path, only shows root page.
    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertNotEquals(getComputedStyle(mainElement).display, 'none');

    // No breadcrumb element, ambient subpage, user subpage and wallpaper
    // subpage are shown.
    const breadcrumbElement =
        routerElement.shadowRoot!.querySelector('personalization-breadcrumb');
    assertFalse(!!breadcrumbElement);

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertFalse(!!ambientSubpage);

    const userSubpage = routerElement.shadowRoot!.querySelector('user-subpage');
    assertFalse(!!userSubpage);

    const wallpaperSubpage =
        routerElement.shadowRoot!.querySelector('wallpaper-subpage');
    assertFalse(!!wallpaperSubpage);
  });
});
