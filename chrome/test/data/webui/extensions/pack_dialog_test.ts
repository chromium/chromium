// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-pack-dialog. */

import 'chrome://extensions/extensions.js';

import type {ExtensionsPackDialogElement, PackDialogDelegate} from 'chrome://extensions/extensions.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {isElementVisible} from './test_util.js';

class MockDelegate implements PackDialogDelegate {
  rootPromise: PromiseResolver<string>|null = null;
  keyPromise: PromiseResolver<string>|null = null;
  rootPath: string|null = null;
  keyPath: string|null = null;
  flag: number|undefined = 0;
  packPromise: PromiseResolver<chrome.developerPrivate.PackDirectoryResponse>|
      null = null;

  choosePackRootDirectory() {
    this.rootPromise = new PromiseResolver();
    return this.rootPromise.promise;
  }

  choosePrivateKeyPath() {
    this.keyPromise = new PromiseResolver();
    return this.keyPromise.promise;
  }

  packExtension(rootPath: string, keyPath: string, flag?: number) {
    this.rootPath = rootPath;
    this.keyPath = keyPath;
    this.flag = flag;
    this.packPromise = new PromiseResolver();
    return this.packPromise.promise;
  }
}

suite('ExtensionPackDialogTests', function() {
  let packDialog: ExtensionsPackDialogElement;
  let mockDelegate: MockDelegate;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockDelegate = new MockDelegate();
    packDialog = document.createElement('extensions-pack-dialog');
    packDialog.delegate = mockDelegate;
    document.body.appendChild(packDialog);
  });

  test('Interaction', async () => {
    const dialogElement = packDialog.$.dialog.getNative();

    assertTrue(isElementVisible(dialogElement));
    assertEquals('', packDialog.$.rootDir.value);
    packDialog.$.rootDirBrowse.click();
    await microtasksFinished();

    assertTrue(!!mockDelegate.rootPromise);
    assertEquals('', packDialog.$.rootDir.value);
    const kRootPath = 'this/is/a/path';

    const promises = [];
    promises.push(mockDelegate.rootPromise.promise.then(function() {
      return microtasksFinished().then(() => {
        assertEquals(kRootPath, packDialog.$.rootDir.value);
      });
    }));

    assertEquals('', packDialog.$.keyFile.value);
    packDialog.$.keyFileBrowse.click();
    await microtasksFinished();
    assertTrue(!!mockDelegate.keyPromise);
    assertEquals('', packDialog.$.keyFile.value);
    const kKeyPath = 'here/is/another/path';

    promises.push(mockDelegate.keyPromise.promise.then(function() {
      return microtasksFinished().then(() => {
        assertEquals(kKeyPath, packDialog.$.keyFile.value);
      });
    }));

    mockDelegate.rootPromise.resolve(kRootPath);
    mockDelegate.keyPromise.resolve(kKeyPath);

    await Promise.all(promises);
    packDialog.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();
    await microtasksFinished();
    assertEquals(kRootPath, mockDelegate.rootPath);
    assertEquals(kKeyPath, mockDelegate.keyPath);
  });

  test('PackSuccess', async () => {
    const dialogElement = packDialog.$.dialog.getNative();

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    await mockDelegate.rootPromise!.promise;
    await microtasksFinished();
    assertEquals(kRootPath, packDialog.$.rootDir.value);
    packDialog.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();
    await microtasksFinished();

    mockDelegate.packPromise!.resolve({
      message: '',
      item_path: '',
      pem_path: '',
      override_flags: 0,
      status: chrome.developerPrivate.PackStatus.SUCCESS,
    });
    await mockDelegate.packPromise!.promise;
    await microtasksFinished();

    const packDialogAlert =
        packDialog.shadowRoot!.querySelector('extensions-pack-dialog-alert')!;
    const alertElement = packDialogAlert.$.dialog.getNative();
    assertTrue(isElementVisible(alertElement));
    assertTrue(isElementVisible(dialogElement));
    assertTrue(!!packDialogAlert.shadowRoot!.querySelector('.action-button'));

    const wait = eventToPromise('close', dialogElement);
    // After 'ok', both dialogs should be closed.
    packDialogAlert.shadowRoot!.querySelector<HTMLElement>(
                                   '.action-button')!.click();

    await wait;
    assertFalse(isElementVisible(alertElement));
    assertFalse(isElementVisible(dialogElement));
  });

  test('PackError', async () => {
    const dialogElement = packDialog.$.dialog.getNative();

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    await mockDelegate.rootPromise!.promise;
    await microtasksFinished();
    assertEquals(kRootPath, packDialog.$.rootDir.value);
    packDialog.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();

    mockDelegate.packPromise!.resolve({
      message: '',
      item_path: '',
      pem_path: '',
      override_flags: 0,
      status: chrome.developerPrivate.PackStatus.ERROR,
    });
    await mockDelegate.packPromise!.promise;
    await microtasksFinished();

    // Make sure new alert and the appropriate buttons are visible.
    const packDialogAlert =
        packDialog.shadowRoot!.querySelector('extensions-pack-dialog-alert')!;
    const alertElement = packDialogAlert.$.dialog.getNative();
    assertTrue(isElementVisible(alertElement));
    assertTrue(isElementVisible(dialogElement));
    assertTrue(!!packDialogAlert.shadowRoot!.querySelector('.action-button'));

    // After cancel, original dialog is still open and values unchanged.
    packDialogAlert.shadowRoot!.querySelector<HTMLElement>(
                                   '.action-button')!.click();
    await microtasksFinished();
    assertFalse(isElementVisible(alertElement));
    assertTrue(isElementVisible(dialogElement));
    assertEquals(kRootPath, packDialog.$.rootDir.value);
  });

  test('PackWarning', async () => {
    const dialogElement = packDialog.$.dialog.getNative();

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    const kOverrideFlags = 1;

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    await mockDelegate.rootPromise!.promise;
    await microtasksFinished();
    assertEquals(kRootPath, packDialog.$.rootDir.value);
    packDialog.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();
    await microtasksFinished();

    mockDelegate.packPromise!.resolve({
      message: '',
      status: chrome.developerPrivate.PackStatus.WARNING,
      item_path: 'item_path',
      pem_path: 'pem_path',
      override_flags: kOverrideFlags,
    });
    await mockDelegate.packPromise!.promise;
    await microtasksFinished();

    // Clear the flag. We expect it to change later.
    mockDelegate.flag = 0;

    // Make sure new alert and the appropriate buttons are visible.
    const packDialogAlert =
        packDialog.shadowRoot!.querySelector('extensions-pack-dialog-alert')!;
    const alertElement = packDialogAlert.$.dialog.getNative();
    assertTrue(isElementVisible(alertElement));
    assertTrue(isElementVisible(dialogElement));
    assertFalse(packDialogAlert.shadowRoot!
                    .querySelector<HTMLElement>('.cancel-button')!.hidden);
    assertFalse(packDialogAlert.shadowRoot!
                    .querySelector<HTMLElement>('.action-button')!.hidden);

    // Make sure "proceed anyway" try to pack extension again.
    const whenClosed = eventToPromise('close', packDialogAlert);
    packDialogAlert.shadowRoot!.querySelector<HTMLElement>(
                                   '.action-button')!.click();
    await whenClosed;
    // Make sure packExtension is called again with the right params.
    assertFalse(isElementVisible(alertElement));
    assertEquals(mockDelegate.flag, kOverrideFlags);
  });
});
