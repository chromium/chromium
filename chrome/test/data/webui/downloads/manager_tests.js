// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../mojo_webui_test_support.js';

import {BrowserProxy, DangerType, States} from 'chrome://downloads/downloads.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDownload, TestDownloadsProxy} from 'chrome://test/downloads/test_support.js';

suite('manager tests', function() {
  /** @type {!downloads.Manager} */
  let manager;

  /** @type {!downloads.mojom.PageHandlerCallbackRouter} */
  let pageRouterProxy;

  /** @type {TestDownloadsProxy} */
  let testBrowserProxy;

  /** @type {!downloads.mojom.PageRemote} */
  let callbackRouterRemote;

  /** @type {CrToastManagerElement} */
  let toastManager;

  setup(function() {
    document.body.innerHTML = '';

    testBrowserProxy = new TestDownloadsProxy();
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    BrowserProxy.instance_ = testBrowserProxy;

    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);

    toastManager = manager.$$('cr-toast-manager');
    assertTrue(!!toastManager);
  });

  test('long URLs elide', async () => {
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: States.COMPLETE,
                                       sinceString: 'Today',
                                       url: 'a'.repeat(1000),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();
    flush();

    const item = manager.$$('downloads-item');
    assertLT(item.$$('#url').offsetWidth, item.offsetWidth);
    assertEquals(300, item.$$('#url').textContent.length);
  });

  test('inserting items at beginning render dates correctly', async () => {
    const countDates = () => {
      const items = manager.shadowRoot.querySelectorAll('downloads-item');
      return Array.from(items).reduce((soFar, item) => {
        return item.$$('div[id=date]:not(:empty)') ? soFar + 1 : soFar;
      }, 0);
    };

    const download1 = createDownload();
    const download2 = createDownload();

    callbackRouterRemote.insertItems(0, [download1, download2]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertEquals(1, countDates());

    callbackRouterRemote.removeItem(0);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertEquals(1, countDates());

    callbackRouterRemote.insertItems(0, [download1]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertEquals(1, countDates());
  });

  test('update', async () => {
    const dangerousDownload = createDownload({
      dangerType: DangerType.DANGEROUS_FILE,
      state: States.DANGEROUS,
    });
    callbackRouterRemote.insertItems(0, [dangerousDownload]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertTrue(!!manager.$$('downloads-item').$$('.dangerous'));

    const safeDownload = Object.assign({}, dangerousDownload, {
      dangerType: DangerType.NOT_DANGEROUS,
      state: States.COMPLETE,
    });
    callbackRouterRemote.updateItem(0, safeDownload);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertFalse(!!manager.$$('downloads-item').$$('.dangerous'));
  });

  test('remove', async () => {
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: States.COMPLETE,
                                       sinceString: 'Today',
                                       url: 'a'.repeat(1000),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    const item = manager.$$('downloads-item');

    item.$.remove.click();
    await testBrowserProxy.handler.whenCalled('remove');
    flush();
    const list = manager.$$('iron-list');
    assertTrue(list.hidden);
  });

  test('toolbar hasClearableDownloads set correctly', async () => {
    const clearable = createDownload();
    callbackRouterRemote.insertItems(0, [clearable]);
    const checkNotClearable = async state => {
      const download = createDownload({state: state});
      callbackRouterRemote.updateItem(0, clearable);
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(manager.$.toolbar.hasClearableDownloads);
      callbackRouterRemote.updateItem(0, download);
      await callbackRouterRemote.$.flushForTesting();
      assertFalse(manager.$.toolbar.hasClearableDownloads);
    };
    await checkNotClearable(States.DANGEROUS);
    await checkNotClearable(States.IN_PROGRESS);
    await checkNotClearable(States.PAUSED);

    callbackRouterRemote.updateItem(0, clearable);
    callbackRouterRemote.insertItems(
        1, [createDownload({state: States.DANGEROUS})]);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(manager.$.toolbar.hasClearableDownloads);
    callbackRouterRemote.removeItem(0);
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(manager.$.toolbar.hasClearableDownloads);
  });

  test('loadTimeData contains isManaged and browserManagedByOrg', function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('browserManagedByOrg');
  });

  test('toast is shown when clear-all-command is fired', async () => {
    // Add a download entry so that clear-all-command is applicable.
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: States.COMPLETE,
                                       sinceString: 'Today',
                                       url: 'a'.repeat(1000),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();

    assertFalse(toastManager.isToastOpen);

    // Simulate 'alt+c' key combo.
    keyDownOn(document, null, 'alt', isMac ? 'ç' : 'c');
    assertTrue(toastManager.isToastOpen);
  });

  test('toast is hidden when undo-command is fired', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);

    // Simulate 'ctrl+z' key combo (or meta+z for Mac).
    keyDownOn(document, null, isMac ? 'meta' : 'ctrl', 'z');
    assertFalse(toastManager.isToastOpen);
  });

  test('toast is hidden when undo is clicked', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    manager.$$('cr-toast-manager cr-button').click();
    assertFalse(toastManager.isToastOpen);
  });

  test('undo is not shown when removing only dangerous items', async () => {
    callbackRouterRemote.insertItems(0, [
      createDownload({isDangerous: true}),
      createDownload({isMixedContent: true})
    ]);
    await callbackRouterRemote.$.flushForTesting();
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    keyDownOn(document, null, 'alt', isMac ? 'ç' : 'c');
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is shown when removing items', async () => {
    callbackRouterRemote.insertItems(0, [
      createDownload(), createDownload({isDangerous: true}),
      createDownload({isMixedContent: true})
    ]);
    await callbackRouterRemote.$.flushForTesting();
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    keyDownOn(document, null, 'alt', isMac ? 'ç' : 'c');
    assertFalse(toastManager.slottedHidden);
  });
});
