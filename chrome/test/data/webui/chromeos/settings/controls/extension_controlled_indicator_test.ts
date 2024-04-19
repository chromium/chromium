// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OpenWindowProxyImpl, ExtensionControlBrowserProxyImpl, ExtensionControlledIndicatorElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {TestExtensionControlBrowserProxy} from '../test_extension_control_browser_proxy.js';
import {clearBody} from '../utils.js';

// clang-format on

suite('extension controlled indicator', function() {
  let browserProxy: TestExtensionControlBrowserProxy;
  let indicator: ExtensionControlledIndicatorElement;

  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    clearBody();
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    indicator = document.createElement('extension-controlled-indicator');
    indicator.extensionId = 'peiafolljookckjknpgofpbjobgbmpge';
    indicator.extensionCanBeDisabled = true;
    indicator.extensionName = 'The Bestest Name Ever';
    document.body.appendChild(indicator);
    flush();
  });

  test('disable button tracks extensionCanBeDisabled', function() {
    assertTrue(indicator.extensionCanBeDisabled);
    assertTrue(!!indicator.shadowRoot!.querySelector('#disable'));

    indicator.extensionCanBeDisabled = false;
    flush();
    assertFalse(!!indicator.shadowRoot!.querySelector('#disable'));
  });

  test('label icon and text', function() {
    const imgSrc = indicator.shadowRoot!.querySelector('img')!.src;
    assertTrue(imgSrc.includes(indicator.extensionId));

    const label = indicator.shadowRoot!.querySelector('span');
    assertTrue(!!label);
    assertTrue(label.textContent!.includes(indicator.extensionName));
  });

  test('tapping manage button invokes browser proxy', async function() {
    const button = indicator.shadowRoot!.querySelector<HTMLElement>('#manage');
    assertTrue(!!button);
    button!.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, `chrome://extensions/?id=${indicator.extensionId}`);
  });

  test('tapping disable button invokes browser proxy', async function() {
    const button = indicator.shadowRoot!.querySelector<HTMLElement>('#disable');
    assertTrue(!!button);
    button!.click();
    const extensionId = await browserProxy.whenCalled('disableExtension');
    assertEquals(extensionId, indicator.extensionId);
  });
});
