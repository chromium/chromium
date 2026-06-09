// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ExtensionControlledMessageElement} from 'chrome://settings/settings.js';
import {OpenWindowProxyImpl, ExtensionControlBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {resetRouterForTesting, loadTimeData} from 'chrome://settings/settings.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';
// clang-format on

suite('extension controlled message', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let browserProxy: TestExtensionControlBrowserProxy;
  let message: ExtensionControlledMessageElement;

  setup(function() {
    loadTimeData.overrideValues({searchSettingsUpdate: true});
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    message = document.createElement('extension-controlled-message');
    message.extensionId = 'peiafolljookckjknpgofpbjobgbmpge';
    message.extensionCanBeDisabled = true;
    message.extensionName = 'The Bestest Name Ever';
    document.body.appendChild(message);
    return flush();
  });

  test('disable link tracks extensionCanBeDisabled', function() {
    assertTrue(message.extensionCanBeDisabled);
    assertTrue(!!message.shadowRoot!.querySelector('#disableLink'));

    message.extensionCanBeDisabled = false;
    flush();
    assertFalse(!!message.shadowRoot!.querySelector('#disableLink'));
  });

  test('tapping manage link invokes browser proxy', async function() {
    const manageLink =
        message.shadowRoot!.querySelector<HTMLElement>('#manageLink');
    assertTrue(!!manageLink);
    assertEquals(
        'Opens in new tab', manageLink.getAttribute('aria-description'));
    manageLink.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, `chrome://extensions/?id=${message.extensionId}`);
  });

  test('tapping disable link invokes browser proxy', async function() {
    const disableLink =
        message.shadowRoot!.querySelector<HTMLElement>('#disableLink');
    assertTrue(!!disableLink);
    disableLink.click();
    const extensionId = await browserProxy.whenCalled('disableExtension');
    assertEquals(extensionId, message.extensionId);
  });
});
