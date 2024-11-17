// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://on-device-translation-internals/app.js';
import type {OnDeviceTranslationInternalsAppElement} from 'chrome://on-device-translation-internals/app.js';
import {BrowserProxy} from 'chrome://on-device-translation-internals/browser_proxy.js';
import {LanguagePackStatus} from 'chrome://on-device-translation-internals/on_device_translation_internals.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOnDeviceTranslationInternalsBrowserProxy} from './test_on_device_translation_internals_browser_proxy.js';

suite('OnDeviceTranslationInternalsTest', function() {
  let app: OnDeviceTranslationInternalsAppElement;
  let testBrowserProxy: TestOnDeviceTranslationInternalsBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestOnDeviceTranslationInternalsBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('on-device-translation-internals-app');
    document.body.appendChild(app);
  });

  // Test that the package table is empty when there are no language packs.
  test('EmptyPackageTable', function() {
    const packageTable = app.shadowRoot!.querySelector('.package-table');
    assert(packageTable);
    assertEquals(0, packageTable.querySelectorAll('.package-tr').length);
    assertEquals(0, packageTable.querySelectorAll('.package-name').length);
    assertEquals(0, packageTable.querySelectorAll('.package-status').length);
    assertEquals(0, packageTable.querySelectorAll('button').length);
  });

  // Test that the package table is populated with the language packs passed
  // via onLanguagePackStatus.
  test('NonEmptyPackageTable', async function() {
    const packageTable = app.shadowRoot!.querySelector('.package-table');
    assert(packageTable);
    testBrowserProxy.callbackRouterRemote.onLanguagePackStatus([
      {
        'name': 'en - es',
        'status': LanguagePackStatus.kNotInstalled,
      },
      {
        'name': 'en - ja',
        'status': LanguagePackStatus.kNotInstalled,
      },
      {
        'name': 'en - zh',
        'status': LanguagePackStatus.kInstalling,
      },
      {
        'name': 'en - zh-Hant',
        'status': LanguagePackStatus.kInstalled,
      },
    ]);
    await microtasksFinished();

    assertEquals(4, packageTable.querySelectorAll('.package-tr').length);
    const packageNameElements = packageTable.querySelectorAll('.package-name');
    const packageStatusElements =
        packageTable.querySelectorAll('.package-status');
    const buttons = packageTable.querySelectorAll('button');
    assertEquals(4, packageNameElements.length);
    assertEquals('en - es', packageNameElements[0]!.textContent);
    assertEquals('en - ja', packageNameElements[1]!.textContent);
    assertEquals('en - zh', packageNameElements[2]!.textContent);
    assertEquals('en - zh-Hant', packageNameElements[3]!.textContent);
    assertEquals(4, packageStatusElements.length);
    assertEquals('Not installed', packageStatusElements[0]!.textContent);
    assertEquals('Not installed', packageStatusElements[1]!.textContent);
    assertEquals('Installing', packageStatusElements[2]!.textContent);
    assertEquals('Installed', packageStatusElements[3]!.textContent);
    assertEquals(4, buttons.length);
    assertEquals('Install', buttons[0]!.textContent);
    assertEquals('Install', buttons[1]!.textContent);
    assertEquals('Uninstall', buttons[2]!.textContent);
    assertEquals('Uninstall', buttons[3]!.textContent);
  });

  // Test that the install button triggers the installLanguagePackage method
  // on the page handler.
  test('InstallPackage', async function() {
    const packageTable = app.shadowRoot!.querySelector('.package-table');
    assert(packageTable);
    testBrowserProxy.callbackRouterRemote.onLanguagePackStatus([
      {
        'name': 'en - es',
        'status': LanguagePackStatus.kNotInstalled,
      },
      {
        'name': 'en - ja',
        'status': LanguagePackStatus.kNotInstalled,
      },
    ]);
    await microtasksFinished();

    const buttons = packageTable.querySelectorAll('button');
    assertEquals(2, buttons.length);
    assertEquals('Install', buttons[0]!.textContent);
    buttons[0]!.click();
    assertEquals(
        0, await testBrowserProxy.handler.whenCalled('installLanguagePackage'));

    testBrowserProxy.handler.resetResolver('installLanguagePackage');

    assertEquals('Install', buttons[1]!.textContent);
    buttons[1]!.click();
    assertEquals(
        1, await testBrowserProxy.handler.whenCalled('installLanguagePackage'));
  });

  // Test that the uninstall button triggers the uninstallLanguagePackage method
  // on the page handler.
  test('UninstallPackage', async function() {
    const packageTable = app.shadowRoot!.querySelector('.package-table');
    assert(packageTable);
    testBrowserProxy.callbackRouterRemote.onLanguagePackStatus([
      {
        'name': 'en - es',
        'status': LanguagePackStatus.kInstalling,
      },
      {
        'name': 'en - ja',
        'status': LanguagePackStatus.kInstalled,
      },
    ]);
    await microtasksFinished();

    const buttons = packageTable.querySelectorAll('button');
    assertEquals(2, buttons.length);
    assertEquals('Uninstall', buttons[0]!.textContent);
    buttons[0]!.click();
    assertEquals(
        0,
        await testBrowserProxy.handler.whenCalled('uninstallLanguagePackage'));

    testBrowserProxy.handler.resetResolver('uninstallLanguagePackage');

    assertEquals('Uninstall', buttons[1]!.textContent);
    buttons[1]!.click();
    assertEquals(
        1,
        await testBrowserProxy.handler.whenCalled('uninstallLanguagePackage'));
  });

  // Test that the package table is updated when onLanguagePackStatus is
  // called.
  test('UpdatePackageTable', async function() {
    const packageTable = app.shadowRoot!.querySelector('.package-table');
    assert(packageTable);
    testBrowserProxy.callbackRouterRemote.onLanguagePackStatus([
      {
        'name': 'en - es',
        'status': LanguagePackStatus.kInstalled,
      },
      {
        'name': 'en - ja',
        'status': LanguagePackStatus.kNotInstalled,
      },
    ]);
    await microtasksFinished();

    assertEquals(2, packageTable.querySelectorAll('.package-tr').length);
    {
      const packageNameElements =
          packageTable.querySelectorAll('.package-name');
      const packageStatusElements =
          packageTable.querySelectorAll('.package-status');
      const buttons = packageTable.querySelectorAll('button');
      assertEquals(2, packageNameElements.length);
      assertEquals('en - es', packageNameElements[0]!.textContent);
      assertEquals('en - ja', packageNameElements[1]!.textContent);
      assertEquals(2, packageStatusElements.length);
      assertEquals('Installed', packageStatusElements[0]!.textContent);
      assertEquals('Not installed', packageStatusElements[1]!.textContent);
      assertEquals(2, buttons.length);
      assertEquals('Uninstall', buttons[0]!.textContent);
      assertEquals('Install', buttons[1]!.textContent);
    }

    testBrowserProxy.callbackRouterRemote.onLanguagePackStatus([
      {
        'name': 'en - es',
        'status': LanguagePackStatus.kInstalled,
      },
      {
        'name': 'en - ja',
        'status': LanguagePackStatus.kInstalling,  // This status is changed.
      },
    ]);
    await microtasksFinished();

    assertEquals(2, packageTable.querySelectorAll('.package-tr').length);
    {
      const packageNameElements =
          packageTable.querySelectorAll('.package-name');
      const packageStatusElements =
          packageTable.querySelectorAll('.package-status');
      const buttons = packageTable.querySelectorAll('button');
      assertEquals(2, packageNameElements.length);
      assertEquals('en - es', packageNameElements[0]!.textContent);
      assertEquals('en - ja', packageNameElements[1]!.textContent);
      assertEquals(2, packageStatusElements.length);
      assertEquals('Installed', packageStatusElements[0]!.textContent);
      assertEquals('Installing', packageStatusElements[1]!.textContent);
      assertEquals(2, buttons.length);
      assertEquals('Uninstall', buttons[0]!.textContent);
      assertEquals('Uninstall', buttons[1]!.textContent);
    }
  });
});
