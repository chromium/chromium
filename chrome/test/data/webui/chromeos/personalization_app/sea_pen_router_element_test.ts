// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenInputQueryElement, SeaPenPaths, SeaPenRecentWallpapersElement, SeaPenRouterElement, SeaPenTemplateQueryElement, SeaPenTemplatesElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';

suite('SeaPenRouterElementTest', function() {
  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    baseSetup();
  });

  test('shows templates and recent elements', async () => {
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});
    router.goToRoute(SeaPenPaths.ROOT);
    await waitAfterNextRender(router);

    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenTemplatesElement.is),
        'sea-pen-templates shown on root');
    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers shown on root');

    assertFalse(
        !!router.shadowRoot!.querySelector(SeaPenInputQueryElement.is),
        'no input query element on root');
    assertFalse(
        !!router.shadowRoot!.querySelector(SeaPenTemplateQueryElement.is),
        'no template query element on root');

    router.selectSeaPenTemplate('123');
    await waitAfterNextRender(router);

    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenTemplateQueryElement.is),
        'template query element is shown after selecting template');
  });

  test('shows input query element if text input enabled', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});
    router.goToRoute(SeaPenPaths.ROOT);
    await waitAfterNextRender(router);

    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenInputQueryElement.is),
        'input query element shown on root');
  });
});
