// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-pack-dialog. */

import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise, flushTasks} from '../test_util.js';

import {isElementVisible} from './test_util.js';

window.extension_pack_dialog_tests = {};
extension_pack_dialog_tests.suiteName = 'ExtensionPackDialogTests';
/** @enum {string} */
extension_pack_dialog_tests.TestNames = {
  Interaction: 'Interaction',
  PackSuccess: 'PackSuccess',
  PackWarning: 'PackWarning',
  PackError: 'PackError',
};

/** @implements {PackDialogDelegate} */
class MockDelegate {
  constructor() {
    this.mockResponse = null;
    this.rootPromise;
    this.keyPromise;
    this.flag = 0;
  }

  /** @override */
  choosePackRootDirectory() {
    this.rootPromise = new PromiseResolver();
    return this.rootPromise.promise;
  }

  /** @override */
  choosePrivateKeyPath() {
    this.keyPromise = new PromiseResolver();
    return this.keyPromise.promise;
  }

  /** @override */
  packExtension(rootPath, keyPath, flag, callback) {
    this.rootPath = rootPath;
    this.keyPath = keyPath;
    this.flag = flag;

    if (callback && this.mockResponse) {
      callback(this.mockResponse);
    }
  }
}

suite(extension_pack_dialog_tests.suiteName, function() {
  /** @type {ExtensionsPackDialogElement} */
  let packDialog;

  /** @type {MockDelegate} */
  let mockDelegate;

  setup(function() {
    document.body.innerHTML = '';
    mockDelegate = new MockDelegate();
    packDialog = document.createElement('extensions-pack-dialog');
    packDialog.delegate = mockDelegate;
    document.body.appendChild(packDialog);
  });

  test(assert(extension_pack_dialog_tests.TestNames.Interaction), function() {
    const dialogElement =
        packDialog.shadowRoot.querySelector('cr-dialog').getNative();

    expectTrue(isElementVisible(dialogElement));
    expectEquals('', packDialog.shadowRoot.querySelector('#root-dir').value);
    packDialog.shadowRoot.querySelector('#root-dir-browse').click();
    expectTrue(!!mockDelegate.rootPromise);
    expectEquals('', packDialog.shadowRoot.querySelector('#root-dir').value);
    const kRootPath = 'this/is/a/path';

    const promises = [];
    promises.push(mockDelegate.rootPromise.promise.then(function() {
      expectEquals(
          kRootPath, packDialog.shadowRoot.querySelector('#root-dir').value);
      expectEquals(kRootPath, packDialog.packDirectory_);
    }));

    flush();
    expectEquals('', packDialog.shadowRoot.querySelector('#key-file').value);
    packDialog.shadowRoot.querySelector('#key-file-browse').click();
    expectTrue(!!mockDelegate.keyPromise);
    expectEquals('', packDialog.shadowRoot.querySelector('#key-file').value);
    const kKeyPath = 'here/is/another/path';

    promises.push(mockDelegate.keyPromise.promise.then(function() {
      expectEquals(
          kKeyPath, packDialog.shadowRoot.querySelector('#key-file').value);
      expectEquals(kKeyPath, packDialog.keyFile_);
    }));

    mockDelegate.rootPromise.resolve(kRootPath);
    mockDelegate.keyPromise.resolve(kKeyPath);

    return Promise.all(promises).then(function() {
      packDialog.shadowRoot.querySelector('.action-button').click();
      expectEquals(kRootPath, mockDelegate.rootPath);
      expectEquals(kKeyPath, mockDelegate.keyPath);
    });
  });

  test(assert(extension_pack_dialog_tests.TestNames.PackSuccess), function() {
    const dialogElement =
        packDialog.shadowRoot.querySelector('cr-dialog').getNative();
    let packDialogAlert;
    let alertElement;

    expectTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      status: chrome.developerPrivate.PackStatus.SUCCESS
    };

    packDialog.shadowRoot.querySelector('#root-dir-browse').click();
    mockDelegate.rootPromise.resolve(kRootPath);

    return mockDelegate.rootPromise.promise
        .then(() => {
          expectEquals(
              kRootPath,
              packDialog.shadowRoot.querySelector('#root-dir').value);
          packDialog.shadowRoot.querySelector('.action-button').click();

          return flushTasks();
        })
        .then(() => {
          packDialogAlert = packDialog.shadowRoot.querySelector(
              'extensions-pack-dialog-alert');
          alertElement = packDialogAlert.$.dialog.getNative();
          expectTrue(isElementVisible(alertElement));
          expectTrue(isElementVisible(dialogElement));
          expectTrue(
              !!packDialogAlert.shadowRoot.querySelector('.action-button'));

          const wait = eventToPromise('close', dialogElement);
          // After 'ok', both dialogs should be closed.
          packDialogAlert.shadowRoot.querySelector('.action-button').click();

          return wait;
        })
        .then(() => {
          expectFalse(isElementVisible(alertElement));
          expectFalse(isElementVisible(dialogElement));
        });
  });

  test(assert(extension_pack_dialog_tests.TestNames.PackError), function() {
    const dialogElement =
        packDialog.shadowRoot.querySelector('cr-dialog').getNative();
    let packDialogAlert;
    let alertElement;

    expectTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      status: chrome.developerPrivate.PackStatus.ERROR
    };

    packDialog.shadowRoot.querySelector('#root-dir-browse').click();
    mockDelegate.rootPromise.resolve(kRootPath);

    return mockDelegate.rootPromise.promise.then(() => {
      expectEquals(
          kRootPath, packDialog.shadowRoot.querySelector('#root-dir').value);
      packDialog.shadowRoot.querySelector('.action-button').click();
      flush();

      // Make sure new alert and the appropriate buttons are visible.
      packDialogAlert =
          packDialog.shadowRoot.querySelector('extensions-pack-dialog-alert');
      alertElement = packDialogAlert.$.dialog.getNative();
      expectTrue(isElementVisible(alertElement));
      expectTrue(isElementVisible(dialogElement));
      expectTrue(!!packDialogAlert.shadowRoot.querySelector('.action-button'));

      // After cancel, original dialog is still open and values unchanged.
      packDialogAlert.shadowRoot.querySelector('.action-button').click();
      flush();
      expectFalse(isElementVisible(alertElement));
      expectTrue(isElementVisible(dialogElement));
      expectEquals(
          kRootPath, packDialog.shadowRoot.querySelector('#root-dir').value);
    });
  });

  test(assert(extension_pack_dialog_tests.TestNames.PackWarning), function() {
    const dialogElement =
        packDialog.shadowRoot.querySelector('cr-dialog').getNative();
    let packDialogAlert;
    let alertElement;

    expectTrue(isElementVisible(dialogElement));

    const kRootPath = 'this/is/a/path';
    mockDelegate.mockResponse = {
      status: chrome.developerPrivate.PackStatus.WARNING,
      item_path: 'item_path',
      pem_path: 'pem_path',
      override_flags: 1,
    };

    packDialog.shadowRoot.querySelector('#root-dir-browse').click();
    mockDelegate.rootPromise.resolve(kRootPath);

    return mockDelegate.rootPromise.promise
        .then(() => {
          expectEquals(
              kRootPath,
              packDialog.shadowRoot.querySelector('#root-dir').value);
          packDialog.shadowRoot.querySelector('.action-button').click();
          flush();

          // Make sure new alert and the appropriate buttons are visible.
          packDialogAlert = packDialog.shadowRoot.querySelector(
              'extensions-pack-dialog-alert');
          alertElement = packDialogAlert.$.dialog.getNative();
          expectTrue(isElementVisible(alertElement));
          expectTrue(isElementVisible(dialogElement));
          expectFalse(packDialogAlert.shadowRoot.querySelector('.cancel-button')
                          .hidden);
          expectFalse(packDialogAlert.shadowRoot.querySelector('.action-button')
                          .hidden);

          // Make sure "proceed anyway" try to pack extension again.
          const whenClosed = eventToPromise('close', packDialogAlert);
          packDialogAlert.shadowRoot.querySelector('.action-button').click();
          return whenClosed;
        })
        .then(() => {
          // Make sure packExtension is called again with the right params.
          expectFalse(isElementVisible(alertElement));
          expectEquals(
              mockDelegate.flag, mockDelegate.mockResponse.override_flags);
        });
  });
});
