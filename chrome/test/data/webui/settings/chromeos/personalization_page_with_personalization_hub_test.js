// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationHubBrowserProxyImpl, Router} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPersonalizationHubBrowserProxy} from './test_personalization_hub_browser_proxy.js';

let personalizationPage = null;

/** @type {?TestPersonalizationHubBrowserProxy} */
let PersonalizationHubBrowserProxy = null;

function createPersonalizationPage() {
  PersonalizationHubBrowserProxy.reset();
  PolymerTest.clearBody();
  personalizationPage = document.createElement('settings-personalization-page');
  document.body.appendChild(personalizationPage);
  flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    PersonalizationHubBrowserProxy = new TestPersonalizationHubBrowserProxy();
    PersonalizationHubBrowserProxyImpl.setInstanceForTesting(
        PersonalizationHubBrowserProxy);
    createPersonalizationPage();
  });

  teardown(function() {
    personalizationPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Personalization hub feature shows only link to hub', async () => {
    createPersonalizationPage();
    flush();
    await waitAfterNextRender(personalizationPage);

    const crLinks =
        personalizationPage.shadowRoot.querySelectorAll('cr-link-row');

    assertEquals(1, crLinks.length);
    assertEquals('personalizationHubButton', crLinks[0].id);
  });

  test('Opens personalization hub when clicked', async () => {
    createPersonalizationPage();
    flush();
    await waitAfterNextRender(personalizationPage);

    const hubLink = personalizationPage.shadowRoot.getElementById(
        'personalizationHubButton');
    hubLink.click();

    await PersonalizationHubBrowserProxy.whenCalled('openPersonalizationHub');
  });
});
