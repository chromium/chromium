// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-pack-dialog. */

import 'chrome://extensions/extensions.js';

import {ExtensionsPackDialogAlertElement, ExtensionsPackDialogElement, PackDialogDelegate} from 'chrome://extensions/extensions.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {isElementVisible} from './test_util.js';

const extension_pack_dialog_tests = {
  suiteName: 'ExtensionPackDialogTests',
  TestNames: {
    Interaction: 'Interaction',
    PackSuccess: 'PackSuccess',
    PackWarning: 'PackWarning',
    PackError: 'PackError',
  },
};

Object.assign(window, {extension_pack_dialog_tests});

class MockDelegate implements PackDialogDelegate {
  rootPromise: PromiseResolver<string>|null = null;
  keyPromise: PromiseResolver<string>|null = null;
  flag: number|undefined = 0;
  mockResponse: chrome.developerPrivate.PackDirectoryResponse|null = null;

  rootPath: string|null = null;
  keyPath: string|null = null;

  choosePackRootDirectory() {
    this.rootPromise = new PromiseResolver();
    return this.rootPromise.promise;
  }

  choosePrivateKeyPath() {
    this.keyPromise = new PromiseResolver();
    return this.keyPromise.promise;
  }

  packExtension(
      rootPath: string, keyPath: string, flag?: number,
      callback?:
          (response: chrome.developerPrivate.PackDirectoryResponse) => void) {
    this.rootPath = rootPath;
    this.keyPath = keyPath;
    this.flag = flag;

    if (callback && this.mockResponse) {
      callback(this.mockResponse);
    }
  }
}

suite(extension_pack_dialog_tests.suiteName, function() {
  let packDialog: ExtensionsPackDialogElement;
  let mockDelegate: MockDelegate;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockDelegate = new MockDelegate();
    packDialog = document.createElement('extensions-pack-dialog');
    packDialog.delegate = mockDelegate;
    document.body.appendChild(packDialog);
  });

  test(extension_pack_dialog_tests.TestNames.Interaction, function() {
    const dialogElement = packDialog.$.dialog.getNative();

    assertTrue(isElementVisible(dialogElement));
    assertEquals('', packDialog.$.rootDir.value);
    packDialog.$.rootDirBrowse.click();
    assertTrue(!!mockDelegate.rootPromise);
    assertEquals('', packDialog.$.rootDir.value);
    const kRootPath = 'this/is/a/path';

    const promises = [];
    promises.push(mockDelegate.rootPromise.promise.then(function() {
      assertEquals(kRootPath, packDialog.$.rootDir.value);
    }));

    flush();
    assertEquals('', packDialog.$.keyFile.value);
    packDialog.$.keyFileBrowse.click();
    assertTrue(!!mockDelegate.keyPromise);
    assertEquals('', packDialog.$.keyFile.value);
    const kKeyPath = 'here/is/another/path';

    promises.push(mockDelegate.keyPromise.promise.then(function() {
      assertEquals(kKeyPath, packDialog.$.keyFile.value);
    }));

    mockDelegate.rootPromise.resolve(kRootPath);
    mockDelegate.keyPromise.resolve(kKeyPath);

    return Promise.all(promises).then(function() {
      packDialog.shadowRoot!.querySelector<HTMLElement>(
                                '.action-button')!.click();
      assertEquals(kRootPath, mockDelegate.rootPath);
      assertEquals(kKeyPath, mockDelegate.keyPath);
    });
  });

  test(extension_pack_dialog_tests.TestNames.PackSuccess, function() {
    const dialogElement = packDialog.$.dialog.getNative();
    let packDialogAlert: ExtensionsPackDialogAlertElement;
    let alertElement: HTMLDialogElement;

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      message: '',
      item_path: '',
      pem_path: '',
      override_flags: 0,
      status: chrome.developerPrivate.PackStatus.SUCCESS,
    };

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    return mockDelegate.rootPromise!.promise
        .then(() => {
          assertEquals(kRootPath, packDialog.$.rootDir.value);
          packDialog.shadowRoot!.querySelector<HTMLElement>(
                                    '.action-button')!.click();

          return flushTasks();
        })
        .then(() => {
          packDialogAlert = packDialog.shadowRoot!.querySelector(
              'extensions-pack-dialog-alert')!;
          alertElement = packDialogAlert.$.dialog.getNative();
          assertTrue(isElementVisible(alertElement));
          assertTrue(isElementVisible(dialogElement));
          assertTrue(
              !!packDialogAlert.shadowRoot!.querySelector('.action-button'));

          const wait = eventToPromise('close', dialogElement);
          // After 'ok', both dialogs should be closed.
          packDialogAlert.shadowRoot!
              .querySelector<HTMLElement>('.action-button')!.click();

          return wait;
        })
        .then(() => {
          assertFalse(isElementVisible(alertElement));
          assertFalse(isElementVisible(dialogElement));
        });
  });

  test(extension_pack_dialog_tests.TestNames.PackError, function() {
    const dialogElement = packDialog.$.dialog.getNative();
    let packDialogAlert: ExtensionsPackDialogAlertElement;
    let alertElement: HTMLDialogElement;

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      message: '',
      item_path: '',
      pem_path: '',
      override_flags: 0,
      status: chrome.developerPrivate.PackStatus.ERROR,
    };

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    return mockDelegate.rootPromise!.promise.then(() => {
      assertEquals(kRootPath, packDialog.$.rootDir.value);
      packDialog.shadowRoot!.querySelector<HTMLElement>(
                                '.action-button')!.click();
      flush();

      // Make sure new alert and the appropriate buttons are visible.
      packDialogAlert =
          packDialog.shadowRoot!.querySelector('extensions-pack-dialog-alert')!;
      alertElement = packDialogAlert.$.dialog.getNative();
      assertTrue(isElementVisible(alertElement));
      assertTrue(isElementVisible(dialogElement));
      assertTrue(!!packDialogAlert.shadowRoot!.querySelector('.action-button'));

      // After cancel, original dialog is still open and values unchanged.
      packDialogAlert.shadowRoot!.querySelector<HTMLElement>(
                                     '.action-button')!.click();
      flush();
      assertFalse(isElementVisible(alertElement));
      assertTrue(isElementVisible(dialogElement));
      assertEquals(kRootPath, packDialog.$.rootDir.value);
    });
  });

  test(extension_pack_dialog_tests.TestNames.PackWarning, function() {
    const dialogElement = packDialog.$.dialog.getNative();
    let packDialogAlert: ExtensionsPackDialogAlertElement;
    let alertElement: HTMLDialogElement;

    assertTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      message: '',
      status: chrome.developerPrivate.PackStatus.WARNING,
      item_path: 'item_path',
      pem_path: 'pem_path',
      override_flags: 1,
    };

    packDialog.$.rootDirBrowse.click();
    mockDelegate.rootPromise!.resolve(kRootPath);

    return mockDelegate.rootPromise!.promise
        .then(() => {
          assertEquals(kRootPath, packDialog.$.rootDir.value);
          packDialog.shadowRoot!.querySelector<HTMLElement>(
                                    '.action-button')!.click();
          flush();

          // Make sure new alert and the appropriate buttons are visible.
          packDialogAlert = packDialog.shadowRoot!.querySelector(
              'extensions-pack-dialog-alert')!;
          alertElement = packDialogAlert.$.dialog.getNative();
          assertTrue(isElementVisible(alertElement));
          assertTrue(isElementVisible(dialogElement));
          assertFalse(
              packDialogAlert.shadowRoot!
                  .querySelector<HTMLElement>('.cancel-button')!.hidden);
          assertFalse(
              packDialogAlert.shadowRoot!
                  .querySelector<HTMLElement>('.action-button')!.hidden);

          // Make sure "proceed anyway" try to pack extension again.
          const whenClosed = eventToPromise('close', packDialogAlert);
          packDialogAlert.shadowRoot!
              .querySelector<HTMLElement>('.action-button')!.click();
          return whenClosed;
        })
        .then(() => {
          // Make sure packExtension is called again with the right params.
          assertFalse(isElementVisible(alertElement));
          assertEquals(
              mockDelegate.flag, mockDelegate.mockResponse!.override_flags);
        });
  });
});
