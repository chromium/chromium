// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrToastManagerElement, DownloadsManagerElement, PageRemote} from 'chrome://downloads/downloads.js';
import {BrowserProxy, DangerType, loadTimeData, State} from 'chrome://downloads/downloads.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDownload, TestDownloadsProxy} from './test_support.js';

suite('manager tests', function() {
  let manager: DownloadsManagerElement;
  let testBrowserProxy: TestDownloadsProxy;
  let callbackRouterRemote: PageRemote;
  let toastManager: CrToastManagerElement;

  setup(function() {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Set height to 100% so that the manager has a non-zero height. This is
    // necessary for the list to know how many items to render. In production
    // this is done by downloads.html.
    document.documentElement.setAttribute('style', 'height: 100%;');
    document.body.setAttribute('style', 'height: 100%;');

    testBrowserProxy = new TestDownloadsProxy();
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    BrowserProxy.setInstance(testBrowserProxy);

    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);

    toastManager = manager.shadowRoot!.querySelector('cr-toast-manager')!;
    assertTrue(!!toastManager);
    return microtasksFinished();
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
    await microtasksFinished();

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
    await microtasksFinished();
    assertEquals(1, countDates());

    callbackRouterRemote.removeItem(0);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    assertEquals(1, countDates());

    callbackRouterRemote.insertItems(0, [download1]);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    assertEquals(1, countDates());
  });

  test('update', async () => {
    const dangerousDownload = createDownload({
      dangerType: DangerType.kDangerousFile,
      state: State.kDangerous,
    });
    callbackRouterRemote.insertItems(0, [dangerousDownload]);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    assertTrue(!!manager.shadowRoot!.querySelector('downloads-item')!
                     .shadowRoot!.querySelector('.dangerous'));

    const safeDownload = Object.assign({}, dangerousDownload, {
      dangerType: DangerType.kNoApplicableDangerType,
      state: State.kComplete,
    });
    callbackRouterRemote.updateItem(0, safeDownload);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
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
    await microtasksFinished();
    const item = manager.shadowRoot!.querySelector('downloads-item')!;

    const quickRemoveButton =
        item.shadowRoot!.querySelector<HTMLElement>('#quick-remove');
    assertTrue(!!quickRemoveButton);
    quickRemoveButton.click();
    await testBrowserProxy.handler.whenCalled('remove');
    const list = manager.shadowRoot!.querySelector('cr-infinite-list')!;
    assertTrue(list.hidden);
    assertTrue(toastManager.isToastOpen);
  });

  test('toolbar hasClearableDownloads set correctly', async () => {
    const clearable = createDownload();
    callbackRouterRemote.insertItems(0, [clearable]);
    const checkClearable = async (state: State) => {
      const download = createDownload({state: state});
      callbackRouterRemote.updateItem(0, download);
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(manager.$.toolbar.hasClearableDownloads);
    };
    await checkClearable(State.kDangerous);
    await checkClearable(State.kInProgress);
    await checkClearable(State.kPaused);
    await checkClearable(State.kComplete);

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
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({dangerousDownloadInterstitial: false});
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
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
        await microtasksFinished();
        const item = manager.shadowRoot!.querySelector('downloads-item');
        assertTrue(!!item);
        item.dispatchEvent(new CustomEvent('save-dangerous-click', {
          bubbles: true,
          composed: true,
          detail: {id: item.data?.id || ''},
        }));
        await callbackRouterRemote.$.flushForTesting();
        const recordOpenId = await testBrowserProxy.handler.whenCalled(
            'recordOpenBypassWarningDialog');
        assertEquals('itemId', recordOpenId);
        const dialog = manager.shadowRoot!.querySelector(
            'downloads-bypass-warning-confirmation-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('item.pdf', dialog.fileName);
        // Confirm the dialog to download the dangerous file.
        dialog.$.dialog.close();
        await callbackRouterRemote.$.flushForTesting();
        const saveDangerousId = await testBrowserProxy.handler.whenCalled(
            'saveDangerousFromDialogRequiringGesture');
        assertEquals('itemId', saveDangerousId);
        assertFalse(dialog.$.dialog.open);
      });

  test('bypass warning confirmation dialog records cancellation', async () => {
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
    await microtasksFinished();
    const item = manager.shadowRoot!.querySelector('downloads-item');
    assertTrue(!!item);
    item.dispatchEvent(new CustomEvent('save-dangerous-click', {
      bubbles: true,
      composed: true,
      detail: {id: item.data?.id || ''},
    }));
    await callbackRouterRemote.$.flushForTesting();
    const recordOpenId = await testBrowserProxy.handler.whenCalled(
        'recordOpenBypassWarningDialog');
    assertEquals('itemId', recordOpenId);
    const dialog = manager.shadowRoot!.querySelector(
        'downloads-bypass-warning-confirmation-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertEquals('item.pdf', dialog.fileName);
    // Cancel the dialog and check that it's recorded.
    dialog.$.dialog.cancel();
    await callbackRouterRemote.$.flushForTesting();
    const recordCancelId = await testBrowserProxy.handler.whenCalled(
        'recordCancelBypassWarningDialog');
    assertEquals('itemId', recordCancelId);
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
        await microtasksFinished();
        const item = manager.shadowRoot!.querySelector('downloads-item')!;
        assertTrue(!!item);
        item.dispatchEvent(new CustomEvent('save-dangerous-click', {
          bubbles: true,
          composed: true,
          detail: {id: item.data?.id || ''},
        }));
        await microtasksFinished();

        const dialog = manager.shadowRoot!.querySelector(
            'downloads-bypass-warning-confirmation-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        // Remove the file and check that the dialog is hidden.
        callbackRouterRemote.removeItem(0);
        await callbackRouterRemote.$.flushForTesting();
        assertFalse(isVisible(dialog));
      });

  test(
      'interstitial shown when dangerousDownloadInterstitial enabled',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({dangerousDownloadInterstitial: true});
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
        callbackRouterRemote.insertItems(0, [
          createDownload({
            id: 'itemId',
            state: State.kDangerous,
            isDangerous: true,
            dangerType: DangerType.kDangerousUrl,
          }),
        ]);
        await callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        const saveDangerousButton =
            manager.shadowRoot!.querySelector('downloads-item')!.shadowRoot!
                .querySelector('cr-action-menu')!.querySelector<HTMLElement>(
                    '#save-dangerous');
        assertTrue(!!saveDangerousButton);
        saveDangerousButton.click();
        const recordOpenId = await testBrowserProxy.handler.whenCalled(
            'recordOpenBypassWarningInterstitial');
        assertEquals('itemId', recordOpenId);
        const interstitial = manager.shadowRoot!.querySelector(
            'downloads-dangerous-download-interstitial');
        assertTrue(!!interstitial);
        assertTrue(interstitial.$.dialog.open);

        const surveyResponse = 'kNoResponse';
        interstitial.$.dialog.close(surveyResponse);
        interstitial.dispatchEvent(new CustomEvent('close', {
          bubbles: true,
          composed: true,
        }));
        const saveDangerousId = await testBrowserProxy.handler.whenCalled(
            'saveDangerousFromInterstitialNeedGesture');
        assertEquals(surveyResponse, interstitial.$.dialog.returnValue);
        assertEquals('itemId', saveDangerousId);
        assertFalse(interstitial.$.dialog.open);
      });

  test('dangerousDownloadInterstitial records cancellation', async () => {
    callbackRouterRemote.insertItems(0, [
      createDownload({
        id: 'itemId',
        state: State.kDangerous,
        isDangerous: true,
        dangerType: DangerType.kDangerousUrl,
      }),
    ]);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const saveDangerousButton =
        manager.shadowRoot!.querySelector('downloads-item')!.shadowRoot!
            .querySelector('cr-action-menu')!.querySelector<HTMLElement>(
                '#save-dangerous');
    assertTrue(!!saveDangerousButton);
    saveDangerousButton.click();
    const recordOpenId = await testBrowserProxy.handler.whenCalled(
        'recordOpenBypassWarningInterstitial');
    assertEquals('itemId', recordOpenId);
    const interstitial = manager.shadowRoot!.querySelector(
        'downloads-dangerous-download-interstitial');
    assertTrue(!!interstitial);
    assertTrue(interstitial.$.dialog.open);

    interstitial.$.dialog.close();
    interstitial.dispatchEvent(new CustomEvent('cancel', {
      bubbles: true,
      composed: true,
    }));
    const recordCancelId = await testBrowserProxy.handler.whenCalled(
        'recordCancelBypassWarningInterstitial');
    assertEquals('itemId', recordCancelId);
    assertFalse(interstitial.$.dialog.open);
  });

  test(
      'bypass warning confirmation interstitial closed when file removed',
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
        await microtasksFinished();

        const item = manager.shadowRoot!.querySelector('downloads-item');
        assertTrue(!!item);
        const saveDangerousButton =
            manager.shadowRoot!.querySelector('downloads-item')!.shadowRoot!
                .querySelector('cr-action-menu')!.querySelector<HTMLElement>(
                    '#save-dangerous');
        assertTrue(!!saveDangerousButton);
        saveDangerousButton.click();
        await microtasksFinished();

        const interstitial = manager.shadowRoot!.querySelector(
            'downloads-dangerous-download-interstitial');
        assertTrue(!!interstitial);
        assertTrue(interstitial.$.dialog.open);
        // Remove the file and check that the interstitial is hidden.
        callbackRouterRemote.removeItem(0);
        await callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        assertFalse(isVisible(interstitial));
      });

  // <if expr="_google_chrome">
  test(
      'shouldShowEsbPromotion returns true on first dangerous download',
      async () => {
        document.body.removeChild(manager);
        loadTimeData.overrideValues({esbDownloadRowPromo: true});
        testBrowserProxy.handler.setEligbleForEsbPromo(true);
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
        const dangerousDownload = createDownload({
          dangerType: DangerType.kDangerousFile,
          state: State.kDangerous,
          isDangerous: true,
        });
        callbackRouterRemote.insertItems(0, [dangerousDownload]);
        await callbackRouterRemote.$.flushForTesting();

        const item = manager.shadowRoot!.querySelector('downloads-item');
        assertTrue(!!item);
        assertTrue(item.showEsbPromotion);
      });

  test(
      'shouldShowEsbPromotion returns true on most recent dangerous download',
      async () => {
        document.body.removeChild(manager);
        loadTimeData.overrideValues({esbDownloadRowPromo: true});
        testBrowserProxy.handler.setEligbleForEsbPromo(true);
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
        const dangerousDownload = createDownload({
          dangerType: DangerType.kDangerousFile,
          state: State.kDangerous,
          isDangerous: true,
          id: 'dangerousdownload1',
        });
        const dangerousDownloadTwo = createDownload({
          dangerType: DangerType.kDangerousFile,
          state: State.kDangerous,
          isDangerous: true,
          url: stringToMojoUrl('http://evil.com'),
          id: 'dangerousdownload2',
        });
        callbackRouterRemote.insertItems(
            0, [dangerousDownload, dangerousDownloadTwo]);
        await callbackRouterRemote.$.flushForTesting();
        const itemList = manager.shadowRoot!.querySelectorAll('downloads-item');
        assertEquals(itemList.length, 2);
        assertTrue(itemList[0]!.showEsbPromotion);
        assertFalse(itemList[1]!.showEsbPromotion);
      });

  test(
      'calls logsEsbPromotionRowViewed when promo row is in first 5 downloads',
      async () => {
        document.body.removeChild(manager);
        loadTimeData.overrideValues({esbDownloadRowPromo: true});
        testBrowserProxy.handler.setEligbleForEsbPromo(true);
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
        const dangerousDownload = createDownload({
          dangerType: DangerType.kDangerousFile,
          state: State.kDangerous,
          isDangerous: true,
          id: 'dangerousdownload',
        });
        const downloadTwo = createDownload({
          dangerType: DangerType.kNoApplicableDangerType,
          state: State.kComplete,
          id: 'download2',
        });
        const downloadThree = createDownload({
          dangerType: DangerType.kNoApplicableDangerType,
          state: State.kComplete,
          id: 'download3',
        });
        const downloadFour = createDownload({
          dangerType: DangerType.kNoApplicableDangerType,
          state: State.kComplete,
          id: 'download4',
        });
        const downloadFive = createDownload({
          dangerType: DangerType.kNoApplicableDangerType,
          state: State.kComplete,
          id: 'download5',
        });
        callbackRouterRemote.insertItems(0, [
          downloadTwo,
          downloadThree,
          downloadFour,
          downloadFive,
          dangerousDownload,
        ]);
        await callbackRouterRemote.$.flushForTesting();
        await testBrowserProxy.handler.whenCalled('logEsbPromotionRowViewed');
      });
  // </if>
});
