// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsSafetyHubExtensionsModuleElement} from 'chrome://settings/lazy_load.js';
import {SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {SettingsPluralStringProxyImpl, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
// clang-format on

suite('CrSettingsSafetyHubExtensionsTest', function() {
  let testElement: SettingsSafetyHubExtensionsModuleElement;
  let pluralString: TestPluralStringProxy;
  let openWindowProxy: TestOpenWindowProxy;

  /**
   * Assert expected plural string is populated. Whenever getPluralString is
   * called, TestPluralStringProxy stacks them in args. If getPluralString is
   * called multiple times, passing 'index' will make the corresponding callback
   * checked.
   */
  async function assertPluralString(
      messageName: string, itemCount: number, index: number = 0) {
    await pluralString.whenCalled('getPluralString');
    const params = pluralString.getArgs('getPluralString')[index];
    await flushTasks();
    assertEquals(messageName, params.messageName);
    assertEquals(itemCount, params.itemCount);
    pluralString.resetResolver('getPluralString');
  }

  setup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return flushTasks();
  });

  test('testExtensionsModule', async function() {
    testElement =
        document.createElement('settings-safety-hub-extensions-module');
    document.body.appendChild(testElement);
    await flushTasks();
    pluralString.resetResolver('getPluralString');

    // Check that the proper string is returned.
    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 1);
    await flushTasks();
    await assertPluralString('safetyCheckExtensionsReviewLabel', 1);

    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 2);
    await flushTasks();
    await assertPluralString('safetyCheckExtensionsReviewLabel', 2);

    // After clicking the review button the user should be navigated
    // to the extensions page.
    testElement.$.reviewButton.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('chrome://extensions', url);
  });
});
