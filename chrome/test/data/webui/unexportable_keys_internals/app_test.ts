// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://unexportable-keys-internals/app.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import type {UnexportableKeysInternalsAppElement} from 'chrome://unexportable-keys-internals/app.js';
import {UnexportableKeysInternalsBrowserProxyImpl} from 'chrome://unexportable-keys-internals/browser_proxy.js';
import type {UnexportableKeysInternalsBrowserProxy} from 'chrome://unexportable-keys-internals/browser_proxy.js';
import type {PageHandlerInterface, UnexportableKeyId, UnexportableKeyInfo} from 'chrome://unexportable-keys-internals/unexportable_keys_internals.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://unexportable-keys-internals/unexportable_keys_internals.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  private keys_: UnexportableKeyInfo[] = [];
  private deleteKeySuccess_ = true;

  constructor() {
    super([
      'getUnexportableKeysInfo',
      'deleteKey',
    ]);
  }

  setGetUnexportableKeysInfoResponse(keys: UnexportableKeyInfo[]) {
    this.keys_ = keys;
  }

  getUnexportableKeysInfo() {
    this.methodCalled('getUnexportableKeysInfo');
    return Promise.resolve({keys: this.keys_});
  }

  setDeleteKeySuccess(success: boolean) {
    this.deleteKeySuccess_ = success;
  }

  deleteKey(keyId: UnexportableKeyId) {
    this.methodCalled('deleteKey', keyId);
    return Promise.resolve({success: this.deleteKeySuccess_});
  }
}

class TestUnexportableKeysInternalsBrowserProxy implements
    UnexportableKeysInternalsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestPageHandler;

  constructor(handler: TestPageHandler) {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = handler;
  }
}

suite('UnexportableKeysInternals', function() {
  let element: UnexportableKeysInternalsAppElement;
  let handler: TestPageHandler;

  const columnCount = 5;

  const sampleKey1: UnexportableKeyInfo = {
    keyId: {keyId: {high: 10n, low: 1n} as any},
    wrappedKey: 'key1_wrapped',
    algorithm: 'ECDSA',
    keyTag: 'tag1',
    creationTime: new Date('2025-01-02T00:00:00Z'),
  };

  const sampleKey2: UnexportableKeyInfo = {
    keyId: {keyId: {high: 20n, low: 2n} as any},
    wrappedKey: 'key2_wrapped',
    algorithm: 'RSA',
    keyTag: 'tag2',
    creationTime: new Date('2025-01-01T00:00:00Z'),
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = new TestPageHandler();
    handler.setGetUnexportableKeysInfoResponse([sampleKey1, sampleKey2]);
    UnexportableKeysInternalsBrowserProxyImpl.setInstance(
        new TestUnexportableKeysInternalsBrowserProxy(handler));

    element = document.createElement('unexportable-keys-internals-app');
    document.body.appendChild(element);
  });

  test('DisplaysKeysTable', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    const rows = element.shadowRoot.querySelectorAll('tr');
    // There should be 3 rows: header + 2 data rows (for `sampleKey1` and
    // `sampleKey2`).
    assertEquals(3, rows.length);

    const row0 = rows[0];
    assertTrue(!!row0);
    const headerCells = row0.querySelectorAll('th');
    assertEquals(columnCount, headerCells.length);
    assertEquals('Wrapped Key', headerCells[0]!.textContent.trim());
    assertEquals('Algorithm', headerCells[1]!.textContent.trim());
    assertEquals('Key Tag', headerCells[2]!.textContent.trim());
    assertEquals('Creation Time', headerCells[3]!.textContent.trim());
    assertEquals('Actions', headerCells[4]!.textContent.trim());

    const row1 = rows[1];
    assertTrue(!!row1);
    const cells1 = row1.querySelectorAll('td');
    assertEquals(columnCount, cells1.length);
    assertEquals(sampleKey1.wrappedKey, cells1[0]!.textContent.trim());
    assertEquals(sampleKey1.algorithm, cells1[1]!.textContent.trim());
    assertEquals(sampleKey1.keyTag, cells1[2]!.textContent.trim());
    assertEquals(
        sampleKey1.creationTime.toLocaleString(),
        cells1[3]!.textContent.trim());

    const row2 = rows[2];
    assertTrue(!!row2);
    const cells2 = row2.querySelectorAll('td');
    assertEquals(columnCount, cells2.length);
    assertEquals(sampleKey2.wrappedKey, cells2[0]!.textContent.trim());
    assertEquals(sampleKey2.algorithm, cells2[1]!.textContent.trim());
    assertEquals(sampleKey2.keyTag, cells2[2]!.textContent.trim());
    assertEquals(
        sampleKey2.creationTime.toLocaleString(),
        cells2[3]!.textContent.trim());
  });

  test('DeleteKeySuccess', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    const deleteButtons = element.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, deleteButtons.length);

    // Make sure subsequent calls to `getUnexportableKeysInfo` only return
    // `sampleKey2`.
    handler.setGetUnexportableKeysInfoResponse([sampleKey2]);

    // Delete the first key.
    deleteButtons[0]!.click();

    const keyId = await handler.whenCalled('deleteKey');
    assertEquals(sampleKey1.keyId, keyId);

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    // Only the second key should remain.
    const rows = element.shadowRoot.querySelectorAll('tr');
    assertEquals(2, rows.length);
    const row1 = rows[1];
    assertTrue(!!row1);
    const cells1 = row1.querySelectorAll('td');
    assertEquals(sampleKey2.wrappedKey, cells1[0]!.textContent.trim());

    // The delete error toast should not be shown after the key has been deleted
    // successfully.
    const deleteErrorToast =
        element.shadowRoot.querySelector<CrToastElement>('#deleteErrorToast');
    assertTrue(!!deleteErrorToast);
    assertFalse(deleteErrorToast.open);
  });

  test('DeleteKeyShowsErrorToastOnFailure', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    const deleteButtons = element.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, deleteButtons.length);

    handler.setDeleteKeySuccess(false);

    // Delete the first key.
    deleteButtons[0]!.click();

    const keyId = await handler.whenCalled('deleteKey');
    assertEquals(sampleKey1.keyId, keyId);

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    const deleteErrorToast =
        element.shadowRoot.querySelector<CrToastElement>('#deleteErrorToast');
    assertTrue(!!deleteErrorToast);
    assertTrue(deleteErrorToast.open);
  });

  test('DeleteKeyHidesErrorToastOnSuccess', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    const deleteErrorToast =
        element.shadowRoot.querySelector<CrToastElement>('#deleteErrorToast');
    assertTrue(!!deleteErrorToast);
    assertFalse(deleteErrorToast.open);

    // Show the toast.
    deleteErrorToast.show();
    assertTrue(deleteErrorToast.open);

    const deleteButtons = element.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, deleteButtons.length);

    // Delete the first key.
    deleteButtons[0]!.click();

    const keyId = await handler.whenCalled('deleteKey');
    assertEquals(sampleKey1.keyId, keyId);

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    // The toast (if shown before) should be immediately hidden after the key
    // has been deleted successfully.
    assertFalse(deleteErrorToast.open);
  });
});
