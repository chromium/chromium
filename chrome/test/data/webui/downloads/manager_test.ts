// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, CrToastManagerElement, DangerType, DownloadsManagerElement, loadTimeData, PageRemote, State} from 'chrome://downloads/downloads.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createDownload, TestDownloadsProxy} from './test_support.js';

suite('manager tests', function() {
  let manager: DownloadsManagerElement;
  let testBrowserProxy: TestDownloadsProxy;
  let callbackRouterRemote: PageRemote;
  let toastManager: CrToastManagerElement;

  setup(function() {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestDownloadsProxy();
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    BrowserProxy.setInstance(testBrowserProxy);

    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);

    toastManager = manager.shadowRoot!.querySelector('cr-toast-manager')!;
    assertTrue(!!toastManager);
  });

  test('long URLs don\'t elide', async () => {
    const url = 'https://' +
        'a'.repeat(1000) + '.com/document.pdf';
    const displayUrl = 'https://' +
        '啊'.repeat(1000) + '.com/document.pdf';
    callbackRouterRemote.insertItems(
        0, [createDownload({
          fileName: 'file name',
          state: State.kComplete,
          sinceString: 'Today',
          url: stringToMojoUrl(url),
          displayUrl: stringToMojoString16(displayUrl),
        })]);
    await callbackRouterRemote.$.flushForTesting();
    flush();

    const item = manager.shadowRoot!.querySelector('downloads-item')!;
    assertLT(item.$.url.offsetWidth, item.offsetWidth);
    assertEquals(displayUrl, item.$.url.textContent);
    assertEquals(url, item.$.url.href);
    assertEquals(url, item.$['file-link'].href);
    assertEquals(url, item.$.url.href);
  });

  test('inserting items at beginning render dates correctly', async () => {
    const countDates = () => {
      const items = manager.shadowRoot!.querySelectorAll('downloads-item');
      return Array.from(items).reduce((soFar, item) => {
        return item.shadowRoot!.querySelector('div[id=date]:not(:empty)') ?
            soFar + 1 :
            soFar;
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
      dangerType: DangerType.kDangerousFile,
      state: State.kDangerous,
    });
    callbackRouterRemote.insertItems(0, [dangerousDownload]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertTrue(!!manager.shadowRoot!.querySelector('downloads-item')!
                     .shadowRoot!.querySelector('.dangerous'));

    const safeDownload = Object.assign({}, dangerousDownload, {
      dangerType: DangerType.kNoApplicableDangerType,
      state: State.kComplete,
    });
    callbackRouterRemote.updateItem(0, safeDownload);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertFalse(!!manager.shadowRoot!.querySelector('downloads-item')!
                      .shadowRoot!.querySelector('.dangerous'));
  });

  test('remove', async () => {
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: State.kComplete,
                                       sinceString: 'Today',
                                       url: stringToMojoUrl('a'.repeat(1000)),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    const item = manager.shadowRoot!.querySelector('downloads-item')!;

    item.getMoreActionsButton().click();
    const removeButton = item.shadowRoot!.querySelector<HTMLElement>('#remove');
    assertTrue(!!removeButton);
    removeButton.click();
    await testBrowserProxy.handler.whenCalled('remove');
    flush();
    const list = manager.shadowRoot!.querySelector('iron-list')!;
    assertTrue(list.hidden);
    assertTrue(toastManager.isToastOpen);
  });

  test('toolbar hasClearableDownloads set correctly', async () => {
    const clearable = createDownload();
    callbackRouterRemote.insertItems(0, [clearable]);
    const checkNotClearable = async (state: State) => {
      const download = createDownload({state: state});
      callbackRouterRemote.updateItem(0, clearable);
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(manager.$.toolbar.hasClearableDownloads);
      callbackRouterRemote.updateItem(0, download);
      await callbackRouterRemote.$.flushForTesting();
      assertFalse(manager.$.toolbar.hasClearableDownloads);
    };
    await checkNotClearable(State.kDangerous);
    await checkNotClearable(State.kInProgress);
    await checkNotClearable(State.kPaused);

    callbackRouterRemote.updateItem(0, clearable);
    callbackRouterRemote.insertItems(
        1, [createDownload({state: State.kDangerous})]);
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
                                       state: State.kComplete,
                                       sinceString: 'Today',
                                       url: stringToMojoUrl('a'.repeat(1000)),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();

    assertFalse(toastManager.isToastOpen);

    // Simulate 'alt+c' key combo.
    keyDownOn(document.documentElement, 0, 'alt', isMac ? 'ç' : 'c');
    assertTrue(toastManager.isToastOpen);
  });

  test('toast is hidden when undo-command is fired', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);

    // Simulate 'ctrl+z' key combo (or meta+z for Mac).
    keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');
    assertFalse(toastManager.isToastOpen);
  });

  test('toast is hidden when undo is clicked', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    manager.shadowRoot!
        .querySelector<HTMLElement>('cr-toast-manager cr-button')!.click();
    assertFalse(toastManager.isToastOpen);
  });

  test('toast is not hidden when itself is clicked', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    toastManager.shadowRoot!.querySelector<HTMLElement>('#toast')!.click();
    assertTrue(toastManager.isToastOpen);
  });

  test('toast is hidden when page is clicked', () => {
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);

    document.body.click();
    assertFalse(toastManager.isToastOpen);
  });

  test('undo is not shown when removing only dangerous items', async () => {
    callbackRouterRemote.insertItems(0, [
      createDownload({isDangerous: true}),
      createDownload({isInsecure: true}),
    ]);
    await callbackRouterRemote.$.flushForTesting();
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    keyDownOn(document.documentElement, 0, 'alt', isMac ? 'ç' : 'c');
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is shown when removing items', async () => {
    callbackRouterRemote.insertItems(0, [
      createDownload(),
      createDownload({isDangerous: true}),
      createDownload({isInsecure: true}),
    ]);
    await callbackRouterRemote.$.flushForTesting();
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    keyDownOn(document.documentElement, 0, 'alt', isMac ? 'ç' : 'c');
    assertFalse(toastManager.slottedHidden);
  });

  test(
      'bypass warning confirmation dialog shown on save-dangerous-click',
      async () => {
        callbackRouterRemote.insertItems(0, [
          createDownload({
            id: 'itemId',
            fileName: 'item.pdf',
            state: State.kDangerous,
            isDangerous: true,
            dangerType: DangerType.kDangerousUrl,
          }),
        ]);
        await callbackRouterRemote.$.flushForTesting();
        flush();
        const item = manager.shadowRoot!.querySelector('downloads-item')!;
        assertTrue(!!item);
        item.dispatchEvent(new CustomEvent('save-dangerous-click', {
          bubbles: true,
          composed: true,
          detail: {id: item.data.id},
        }));
        flush();
        const dialog = manager.shadowRoot!.querySelector(
            'download-bypass-warning-confirmation-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('item.pdf', dialog.fileName);
        // Confirm the dialog to download the dangerous file.
        dialog.$.dialog.close();
        flush();
        const id = await testBrowserProxy.handler.whenCalled(
            'saveDangerousRequiringGesture');
        assertEquals('itemId', id);
        assertFalse(dialog.$.dialog.open);
      });

  test(
      'bypass warning confirmation dialog closed when file removed',
      async () => {
        callbackRouterRemote.insertItems(0, [
          createDownload({
            id: 'itemId',
            state: State.kDangerous,
            isDangerous: true,
            dangerType: DangerType.kDangerousUrl,
          }),
        ]);
        await callbackRouterRemote.$.flushForTesting();
        flush();
        const item = manager.shadowRoot!.querySelector('downloads-item')!;
        assertTrue(!!item);
        item.dispatchEvent(new CustomEvent('save-dangerous-click', {
          bubbles: true,
          composed: true,
          detail: {id: item.data.id},
        }));
        flush();
        const dialog = manager.shadowRoot!.querySelector(
            'download-bypass-warning-confirmation-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        // Remove the file and check that the dialog is hidden.
        callbackRouterRemote.removeItem(0);
        await callbackRouterRemote.$.flushForTesting();
        flush();
        assertFalse(isVisible(dialog));
      });
});
