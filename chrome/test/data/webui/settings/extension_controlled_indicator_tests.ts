// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ExtensionControlBrowserProxyImpl, ExtensionControlledIndicatorElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';

// clang-format on

suite('extension controlled indicator', function() {
  let browserProxy: TestExtensionControlBrowserProxy;
  let indicator: ExtensionControlledIndicatorElement;

  setup(function() {
    document.body.innerHTML = '';
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    indicator = document.createElement('extension-controlled-indicator');
    indicator.extensionId = 'peiafolljookckjknpgofpbjobgbmpge';
    indicator.extensionCanBeDisabled = true;
    indicator.extensionName = 'The Bestest Name Ever';
    document.body.appendChild(indicator);
    flush();
  });

  test('disable button tracks extensionCanBeDisabled', function() {
    assertTrue(indicator.extensionCanBeDisabled);
    assertTrue(!!indicator.shadowRoot!.querySelector('cr-button'));

    indicator.extensionCanBeDisabled = false;
    flush();
    assertFalse(!!indicator.shadowRoot!.querySelector('cr-button'));
  });

  test('label text and href', function() {
    let imgSrc = indicator.shadowRoot!.querySelector('img')!.src;
    assertTrue(imgSrc.includes(indicator.extensionId));

    let label = indicator.shadowRoot!.querySelector('span');
    assertTrue(!!label);
    let labelLink = label!.querySelector('a');
    assertTrue(!!labelLink);
    assertEquals(labelLink!.textContent, indicator.extensionName);

    assertEquals('chrome://extensions', new URL(labelLink!.href).origin);
    assertTrue(labelLink!.href.includes(indicator.extensionId));

    indicator.extensionId = 'dpjamkmjmigaoobjbekmfgabipmfilij';
    indicator.extensionName = 'A Slightly Less Good Name (Can\'t Beat That ^)';
    flush();

    imgSrc = indicator.shadowRoot!.querySelector('img')!.src;
    assertTrue(imgSrc.includes(indicator.extensionId));

    label = indicator.shadowRoot!.querySelector('span');
    assertTrue(!!label);
    labelLink = label!.querySelector('a');
    assertTrue(!!labelLink);
    assertEquals(labelLink!.textContent, indicator.extensionName);
  });

  test('tapping disable button invokes browser proxy', function() {
    const disableButton = indicator.shadowRoot!.querySelector('cr-button');
    assertTrue(!!disableButton);
    disableButton!.click();
    return browserProxy.whenCalled('disableExtension')
        .then(function(extensionId) {
          assertEquals(extensionId, indicator.extensionId);
        });
  });
});
