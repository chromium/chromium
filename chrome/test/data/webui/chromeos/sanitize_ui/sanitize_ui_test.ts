// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



import {CrExpandButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {SanitizeDoneElement} from 'chrome://sanitize/sanitize_done.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';


function initSanitizeDoneElement(): SanitizeDoneElement {
  const element = new SanitizeDoneElement();
  document.body.appendChild(element);
  flushTasks();
  return element;
}
suite('SanitizeUITest', function() {
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  test('SanitizeDonePopulation', () => {
    const doneElement = initSanitizeDoneElement();
    const titleDiv = doneElement.shadowRoot!.querySelector('#title');
    // Verify the header element exists
    assert(titleDiv);
    // Check the header content
    assertEquals('Safety reset has been completed', titleDiv!.textContent);
  });

  test('SanitizeDoneAccordionsAndLinksTest', async () => {
    const doneElement = initSanitizeDoneElement();
    // Check accordions exist
    const extensionsAccordion =
        doneElement.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandExtensionsInfo');
    assert(!!extensionsAccordion);
    const chromeOSSettingsAccordion =
        doneElement.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandChromeOsSettingsInfo');
    assert(!!chromeOSSettingsAccordion);
    const chromeSettingsAccordion =
        doneElement.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandChromeSettingsInfo');
    assert(!!chromeSettingsAccordion);

    // Check buttons are hidden under accordions and clicking them opens the
    // relevant link

    // Extensions Section
    const hiddenExtensionsButton =
        doneElement.shadowRoot!.querySelector('#extensionsButton');
    assert(!hiddenExtensionsButton);
    extensionsAccordion.click();
    flushTasks();
    const extensionsButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>('#extensionsButton');
    assert(!!extensionsButton);
    extensionsButton.click();
    const extensionsUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(extensionsUrl, 'chrome://extensions');
    openWindowProxy.resetResolver('openUrl');


    // ChromeOS Settings Section
    const hiddenChromeOsInputButton =
        doneElement.shadowRoot!.querySelector('#chromeOsInputButton');
    const hiddenChromeOsNetworkButton =
        doneElement.shadowRoot!.querySelector('#chromeOsNetworkButton');
    assert(!hiddenChromeOsInputButton);
    assert(!hiddenChromeOsNetworkButton);
    chromeOSSettingsAccordion.click();
    flushTasks();
    const chromeOsInputButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeOsInputButton');
    const chromeOsNetworkButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeOsNetworkButton');

    assert(!!chromeOsInputButton);
    chromeOsInputButton.click();
    const chromeOsInputUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeOsInputUrl, 'chrome://os-settings/osLanguages/input');
    openWindowProxy.resetResolver('openUrl');


    assert(!!chromeOsNetworkButton);
    chromeOsNetworkButton.click();
    const chromeOsNetworkUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeOsNetworkUrl, 'chrome://os-settings/internet');
    openWindowProxy.resetResolver('openUrl');

    // ChromeOS Settings Section
    const hiddenChromeSiteContentButton =
        doneElement.shadowRoot!.querySelector('#chromeSiteContentButton');
    const hiddenChromeStartupButton =
        doneElement.shadowRoot!.querySelector('#chromeStartupButton');
    const hiddenChromeHomepageButton =
        doneElement.shadowRoot!.querySelector('#chromeHomepageButton');
    const hiddenChromeLanguagesButton =
        doneElement.shadowRoot!.querySelector('#chromeLanguagesButton');
    assert(!hiddenChromeSiteContentButton);
    assert(!hiddenChromeStartupButton);
    assert(!hiddenChromeHomepageButton);
    assert(!hiddenChromeLanguagesButton);
    chromeSettingsAccordion.click();
    flushTasks();
    const chromeSiteContentButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeSiteContentButton');
    const chromeStartupButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeStartupButton');
    const chromeHomepageButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeHomepageButton');
    const chromeLanguagesButton =
        doneElement.shadowRoot!.querySelector<HTMLElement>(
            '#chromeLanguagesButton');

    assert(!!chromeSiteContentButton);
    chromeSiteContentButton.click();
    const chromeSiteContentUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeSiteContentUrl, 'chrome://settings/content');
    openWindowProxy.resetResolver('openUrl');

    assert(!!chromeStartupButton);
    chromeStartupButton.click();
    const chromeStartupUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeStartupUrl, 'chrome://settings/onStartup');
    openWindowProxy.resetResolver('openUrl');

    assert(!!chromeHomepageButton);
    chromeHomepageButton.click();
    const chromeHomepageUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeHomepageUrl, 'chrome://settings/appearance');
    openWindowProxy.resetResolver('openUrl');

    assert(!!chromeLanguagesButton);
    chromeLanguagesButton.click();
    const chromeLanguagesUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(chromeLanguagesUrl, 'chrome://settings/languages');
  });
});
