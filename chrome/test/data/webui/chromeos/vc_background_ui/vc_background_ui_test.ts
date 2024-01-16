// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WallpaperGridItemElement} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {getSeaPenTemplates} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {SeaPenRouterElement} from 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';
import {SeaPenTemplateQueryElement} from 'chrome://resources/ash/common/sea_pen/sea_pen_template_query_element.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('VcBackgroundUITest', () => {
  function getSeaPenRouter(): SeaPenRouterElement {
    const seaPenRouter = document.body.querySelector('sea-pen-router');
    assertTrue(!!seaPenRouter, 'sea-pen-router exists');
    return seaPenRouter;
  }

  function getSeaPenTemplateQuery(): SeaPenTemplateQueryElement|null {
    return getSeaPenRouter().shadowRoot!.querySelector(
        'sea-pen-template-query');
  }

  function getSeaPenTemplateElements(): WallpaperGridItemElement[] {
    const seaPenTemplates =
        getSeaPenRouter().shadowRoot?.querySelector('sea-pen-templates');
    assertTrue(!!seaPenTemplates, 'sea-pen-templates exists');
    return Array.from(seaPenTemplates.shadowRoot!.querySelectorAll(
        'wallpaper-grid-item:not([hidden])'));
  }

  test('shows template options when template is clicked', async () => {
    assertFalse(
        !!getSeaPenTemplateQuery(),
        'sea-pen-template-query does not exist yet');

    const seaPenTemplateElements = getSeaPenTemplateElements();
    assertEquals(
        getSeaPenTemplates().length, seaPenTemplateElements.length,
        'each sea pen template is displayed');

    // Click the 'Classic Art' template.
    seaPenTemplateElements
        .find(template => {
          const p = template.shadowRoot?.querySelector('p.primary-text');
          return p?.textContent?.trim() === 'Classic Art';
        })!.click();
    await waitAfterNextRender(getSeaPenRouter());

    const seaPenTemplateQuery = getSeaPenTemplateQuery();
    assertTrue(!!seaPenTemplateQuery, 'sea-pen-template-query exists');

    assertEquals(
        'A painting of a canyon in the avant garde style',
        seaPenTemplateQuery?.shadowRoot?.getElementById('template')
            ?.textContent?.trim()
            .replace(/\s+/g, ' '),
        'Expected template text is shown for Classic Art');

    assertEquals(
        'chrome://vc-background/?seaPenTemplateId=4', window.location.href,
        'Classic Art template id is added to url');
  });
});
