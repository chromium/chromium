// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LacrosExtensionControlBrowserProxyImpl, LacrosExtensionControlledIndicatorElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestLacrosExtensionControlBrowserProxy} from './test_lacros_extension_control_browser_proxy.js';
import {clearBody} from './utils.js';

suite('lacros extension controlled indicator', () => {
  let browserProxy: TestLacrosExtensionControlBrowserProxy;
  let indicator: LacrosExtensionControlledIndicatorElement;

  setup(() => {
    clearBody();
    browserProxy = new TestLacrosExtensionControlBrowserProxy();
    LacrosExtensionControlBrowserProxyImpl.setInstance(browserProxy);

    indicator = document.createElement('lacros-extension-controlled-indicator');
    indicator.extensionId = 'peiafolljookckjknpgofpbjobgbmpge';
    indicator.extensionName = 'The Bestest Name Ever';
    document.body.appendChild(indicator);
    flush();
  });

  teardown(() => {
    indicator.remove();
    browserProxy.reset();
  });

  test('label text', () => {
    const label = indicator.shadowRoot!.querySelector('span');
    assertTrue(!!label);
    assertTrue(label.textContent!.includes(indicator.extensionName));
  });

  test('tapping manage button invokes browser proxy', async () => {
    const button = indicator.shadowRoot!.querySelector<HTMLElement>('#manage');
    assertTrue(!!button);
    button!.click();
    const extensionId = await browserProxy.whenCalled('manageLacrosExtension');
    assertEquals(extensionId, indicator.extensionId);
  });
});
