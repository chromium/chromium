// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationMain} from 'chrome://personalization/trusted/personalization_main_element.js';
import {Paths, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_router_element.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function PersonalizationMainTest() {
  let personalizationMainElement: PersonalizationMain|null;

  setup(function() {});

  teardown(async () => {
    await teardownElement(personalizationMainElement);
    personalizationMainElement = null;
  });

  test('links to user subpage', async () => {
    personalizationMainElement = initElement(PersonalizationMain);
    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          }
        } as PersonalizationRouter;
      };
    });
    const userSubpageLink =
        personalizationMainElement!.shadowRoot!.getElementById(
            'userSubpageLink')!;
    userSubpageLink.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.User, path);
    assertDeepEquals({}, queryParams);
  });

  test('links to ambient subpage', async () => {
    personalizationMainElement = initElement(PersonalizationMain);
    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          }
        } as PersonalizationRouter;
      };
    });
    const ambientSubpageLink =
        personalizationMainElement!.shadowRoot!.getElementById(
            'ambientSubpageLink')!;
    ambientSubpageLink.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.Ambient, path);
    assertDeepEquals({}, queryParams);
  });
}
