// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A test of the extension_controlled_icon, modeled after the
 * equivalent extension_controlled_indicator_tests in settings.
 */

// clang-format off
import 'chrome://password-manager/password_manager.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OpenWindowProxyImpl, ExtensionControlBrowserProxyImpl, ExtensionControlledIconElement} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';

// clang-format on

suite('ExtensionControlledIconTest', function() {
  let browserProxy: TestExtensionControlBrowserProxy;
  let extensionControlledIcon: ExtensionControlledIconElement;

  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    extensionControlledIcon =
        document.createElement('extension-controlled-icon');
    extensionControlledIcon.extensionId = 'peiafolljookckjknpgofpbjobgbmpge';
    extensionControlledIcon.extensionCanBeDisabled = true;
    extensionControlledIcon.extensionName = 'The Bestest Name Ever';
    document.body.appendChild(extensionControlledIcon);
    flush();
  });

  test('disable button tracks extensionCanBeDisabled', function() {
    assertTrue(extensionControlledIcon.extensionCanBeDisabled);
    assertTrue(!!extensionControlledIcon.shadowRoot!.querySelector('#disable'));

    extensionControlledIcon.extensionCanBeDisabled = false;
    flush();
    assertFalse(
        !!extensionControlledIcon.shadowRoot!.querySelector('#disable'));
  });

  test('label icon and text', function() {
    const imgSrc =
        extensionControlledIcon.shadowRoot!.querySelector('img')!.src;
    assertTrue(imgSrc.includes(extensionControlledIcon.extensionId));

    const label = extensionControlledIcon.shadowRoot!.querySelector('span');
    assertTrue(!!label);
    assertTrue(
        label.textContent!.includes(extensionControlledIcon.extensionName));
  });

  test('tapping manage button invokes browser proxy', async function() {
    const button =
        extensionControlledIcon.shadowRoot!.querySelector<HTMLElement>(
            '#manage');
    assertTrue(!!button);
    button.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        url, `chrome://extensions/?id=${extensionControlledIcon.extensionId}`);
  });

  test('tapping disable button invokes browser proxy', async function() {
    const button =
        extensionControlledIcon.shadowRoot!.querySelector<HTMLElement>(
            '#disable');
    assertTrue(!!button);
    button.click();
    const extensionId = await browserProxy.whenCalled('disableExtension');
    assertEquals(extensionId, extensionControlledIcon.extensionId);
  });
});
