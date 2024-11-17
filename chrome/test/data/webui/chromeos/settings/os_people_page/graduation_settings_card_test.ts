// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {GraduationSettingsCardElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, setGraduationHandlerProviderForTesting, settingMojom} from 'chrome://os-settings/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestGraduationHandler} from './test_graduation_handler_provider.js';

suite('GraduationSettingsCard', () => {
  let card: GraduationSettingsCardElement;
  let handler: TestGraduationHandler;

  setup(() => {
    handler = new TestGraduationHandler();
    setGraduationHandlerProviderForTesting(handler);
    card = new GraduationSettingsCardElement();
    document.body.appendChild(card);
    Router.getInstance().navigateTo(routes.OS_PEOPLE);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    card.remove();
  });

  test('Clicking launch app calls launch Graduation app', () => {
    assertEquals(handler.getCallCount('launchGraduationApp'), 0);
    const graduationRow = card.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!graduationRow);
    graduationRow.click();
    assertEquals(handler.getCallCount('launchGraduationApp'), 1);
  });

  test('Search deep links to Graduation link icon', async () => {
    const params = new URLSearchParams();
    const graduationSettingId = settingMojom.Setting.kGraduation.toString();
    params.append('settingId', graduationSettingId);
    Router.getInstance().navigateTo(routes.OS_PEOPLE, params);

    flush();

    const graduationRow = card.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!graduationRow);
    assertTrue(isVisible(graduationRow));

    const linkIcon = graduationRow.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!linkIcon);
    assertTrue(isVisible(linkIcon));
    await waitAfterNextRender(linkIcon);

    assertEquals(linkIcon, getDeepActiveElement());
  });
});
