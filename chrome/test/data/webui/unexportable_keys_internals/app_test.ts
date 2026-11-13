// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://unexportable-keys-internals/app.js';

import type {UnexportableKeysInternalsAppElement} from 'chrome://unexportable-keys-internals/app.js';
import {browserProxyFactory} from 'chrome://unexportable-keys-internals/unexportable_keys_internals.mojom-webui.js';
import type {PageHandlerInterface, UnexportableKeyInfo, UnexportableSigningKeyId} from 'chrome://unexportable-keys-internals/unexportable_keys_internals.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  private keys_: UnexportableKeyInfo[] = [];
  private deleteKeysSuccess_ = true;

  constructor() {
    super([
      'getUnexportableKeysInfo',
      'deleteKeys',
    ]);
  }

  setGetUnexportableKeysInfoResponse(keys: UnexportableKeyInfo[]) {
    this.keys_ = keys;
  }

  getUnexportableKeysInfo() {
    this.methodCalled('getUnexportableKeysInfo');
    return Promise.resolve({keys: this.keys_});
  }

  setDeleteKeysSuccess(success: boolean) {
    this.deleteKeysSuccess_ = success;
  }

  deleteKeys(keyIds: UnexportableSigningKeyId[]) {
    this.methodCalled('deleteKeys', keyIds);
    return Promise.resolve({success: this.deleteKeysSuccess_});
  }
}

suite('UnexportableKeysInternals', function() {
  let element: UnexportableKeysInternalsAppElement;
  let handler: TestPageHandler;

  const columnCount = 5;

  const UNGUESSABLE_TOKEN_LENGTH = 32;

  const sampleKey1: UnexportableKeyInfo = {
    keyId: {keyId: 1n.toString(16).padStart(UNGUESSABLE_TOKEN_LENGTH, '0')},
    wrappedKey: 'key1_wrapped',
    algorithm: 'ECDSA',
    keyTag: 'tag1',
    creationTime: new Date('2025-01-02T00:00:00Z'),
  };

  const sampleKey2: UnexportableKeyInfo = {
    keyId: {keyId: 2n.toString(16).padStart(UNGUESSABLE_TOKEN_LENGTH, '0')},
    wrappedKey: 'key2_wrapped',
    algorithm: 'RSA',
    keyTag: 'tag2',
    creationTime: new Date('2025-01-01T00:00:00Z'),
  };

  const untaggedKey: UnexportableKeyInfo = {
    keyId: {keyId: 3n.toString(16).padStart(UNGUESSABLE_TOKEN_LENGTH, '0')},
    wrappedKey: 'key3_wrapped',
    algorithm: 'ECDSA',
    keyTag: '',
    creationTime: new Date('2025-01-03T00:00:00Z'),
  };

  const originalConfirm = window.confirm;

  function createElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('unexportable-keys-internals-app');
    document.body.appendChild(element);
  }

  function setTagPrefix(tagPrefix: string) {
    const input =
        element.shadowRoot.querySelector<HTMLInputElement>('#tagPrefixInput');
    assertTrue(!!input);
    input.value = tagPrefix;
    input.dispatchEvent(new Event('input'));
  }

  function getDeleteMatchingKeysButton(): HTMLElement {
    const button = element.shadowRoot.querySelector<HTMLElement>(
        '#deleteMatchingKeysButton');
    assertTrue(!!button);
    return button;
  }

  setup(function() {
    handler = new TestPageHandler();
    handler.setGetUnexportableKeysInfoResponse([sampleKey1, sampleKey2]);
    browserProxyFactory.setInstance({handler});
    createElement();
  });

  teardown(function() {
    window.confirm = originalConfirm;
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

    assertDeepEquals(
        [sampleKey1.keyId], await handler.whenCalled('deleteKeys'));

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
    const deleteErrorToast = element.$.deleteErrorToast;
    assertFalse(deleteErrorToast.open);
  });

  test('DeleteKeyShowsErrorToastOnFailure', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    const deleteButtons = element.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, deleteButtons.length);

    handler.setDeleteKeysSuccess(false);

    // Delete the first key.
    deleteButtons[0]!.click();

    assertDeepEquals(
        [sampleKey1.keyId], await handler.whenCalled('deleteKeys'));

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    const deleteErrorToast = element.$.deleteErrorToast;
    assertTrue(deleteErrorToast.open);
  });

  test('DeleteKeyHidesErrorToastOnSuccess', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    const deleteErrorToast = element.$.deleteErrorToast;
    assertFalse(deleteErrorToast.open);

    // Show the toast.
    deleteErrorToast.show();
    assertTrue(deleteErrorToast.open);

    const deleteButtons = element.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, deleteButtons.length);

    // Delete the first key.
    deleteButtons[0]!.click();

    assertDeepEquals(
        [sampleKey1.keyId], await handler.whenCalled('deleteKeys'));

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    // The toast (if shown before) should be immediately hidden after the key
    // has been deleted successfully.
    assertFalse(deleteErrorToast.open);
  });

  test('DeleteMatchingKeysButtonCountsMatchingKeys', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    // An empty prefix only matches keys without a tag.
    const button = getDeleteMatchingKeysButton();
    assertEquals('Delete matching keys (0)', button.textContent.trim());
    assertTrue(button.hasAttribute('disabled'));

    setTagPrefix('tag');
    await microtasksFinished();
    assertEquals('Delete matching keys (2)', button.textContent.trim());
    assertFalse(button.hasAttribute('disabled'));

    setTagPrefix('tag1');
    await microtasksFinished();
    assertEquals('Delete matching keys (1)', button.textContent.trim());
  });

  test('DeleteMatchingKeysAsksForConfirmation', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    setTagPrefix('tag');
    await microtasksFinished();

    let confirmMessage: string|undefined;
    window.confirm = (message?: string) => {
      confirmMessage = message;
      return false;
    };
    getDeleteMatchingKeysButton().click();
    await microtasksFinished();

    assertEquals('Are you sure you want to delete 2 keys?', confirmMessage);
    assertEquals(0, handler.getCallCount('deleteKeys'));
  });

  test('DeleteMatchingKeysSuccess', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');
    setTagPrefix('tag1');
    await microtasksFinished();

    // Make sure subsequent calls to `getUnexportableKeysInfo` only return
    // `sampleKey2`.
    handler.setGetUnexportableKeysInfoResponse([sampleKey2]);

    window.confirm = () => true;
    getDeleteMatchingKeysButton().click();

    assertDeepEquals(
        [sampleKey1.keyId], await handler.whenCalled('deleteKeys'));

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    // Only the second key should remain.
    const rows = element.shadowRoot.querySelectorAll('tr');
    assertEquals(2, rows.length);
    assertFalse(element.$.deleteErrorToast.open);
  });

  test('DeleteMatchingKeysWithEmptyPrefix', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    handler.resetResolver('getUnexportableKeysInfo');
    handler.setGetUnexportableKeysInfoResponse(
        [sampleKey1, sampleKey2, untaggedKey]);
    createElement();
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');

    let confirmMessage: string|undefined;
    window.confirm = (message?: string) => {
      confirmMessage = message;
      return true;
    };
    getDeleteMatchingKeysButton().click();

    assertDeepEquals(
        [untaggedKey.keyId], await handler.whenCalled('deleteKeys'));
    assertEquals('Are you sure you want to delete 1 key?', confirmMessage);

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
  });

  test('DeleteMatchingKeysShowsErrorToastOnFailure', async function() {
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();
    handler.resetResolver('getUnexportableKeysInfo');
    setTagPrefix('tag');
    await microtasksFinished();

    handler.setDeleteKeysSuccess(false);
    window.confirm = () => true;
    getDeleteMatchingKeysButton().click();

    assertDeepEquals(
        [sampleKey1.keyId, sampleKey2.keyId],
        await handler.whenCalled('deleteKeys'));

    // Should trigger a re-fetch once the delete is resolved.
    await handler.whenCalled('getUnexportableKeysInfo');
    await microtasksFinished();

    assertTrue(element.$.deleteErrorToast.open);
  });
});
