// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrIconElement, CrToastManagerElement, DownloadsItemElement} from 'chrome://downloads/downloads.js';
import {BrowserProxy, DangerType, IconLoaderImpl, loadTimeData, SafeBrowsingState, State, TailoredWarningType} from 'chrome://downloads/downloads.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDownload, TestDownloadsProxy, TestIconLoader} from './test_support.js';

suite('ItemTest', function() {
  let item: DownloadsItemElement;
  let testDownloadsProxy: TestDownloadsProxy;
  let testIconLoader: TestIconLoader;
  let toastManager: CrToastManagerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testDownloadsProxy = new TestDownloadsProxy();

    BrowserProxy.setInstance(testDownloadsProxy);

    testIconLoader = new TestIconLoader();
    IconLoaderImpl.setInstance(testIconLoader);

    item = document.createElement('downloads-item');
    document.body.appendChild(item);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('dangerous downloads aren\'t linkable', async () => {
    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kDangerous,
      url: 'http://evil.com',
    });
    await microtasksFinished();

    assertFalse(isVisible(item.$.fileLink));
    assertFalse(item.$.fileLink.hasAttribute('href'));
  });

  test('initiator origin empty string in data isn\'t displayed', async () => {
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.data = createDownload({
      hideDate: false,
      state: State.kComplete,
      displayInitiatorOrigin: '',
    });
    await microtasksFinished();

    assertFalse(isVisible(
        item.shadowRoot.querySelector<HTMLElement>('#initiator-origin')));
  });

  test('initiator origin on dangerous downloads is displayed', async () => {
    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kDangerous,
      displayInitiatorOrigin: 'https://displaytest.com',
    });
    await microtasksFinished();

    assertTrue(isVisible(
        item.shadowRoot.querySelector<HTMLElement>('#initiator-origin')));
  });

  test('failed deep scans display initiator origin', async () => {
    item.data = createDownload({
      dangerType: DangerType.kDeepScannedFailed,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kComplete,
      url: 'http://evil.com',
      displayInitiatorOrigin: 'http://display.com',
    });
    await microtasksFinished();

    assertTrue(isVisible(
        item.shadowRoot.querySelector<HTMLElement>('#initiator-origin')));
  });


  test('icon loads successfully', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.data = createDownload({filePath: 'unique1', hideDate: false});
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique1');
    await microtasksFinished();
    assertFalse(item.getFileIcon().hidden);
  });

  test('icon fails to load', async () => {
    testIconLoader.setShouldIconsLoad(false);
    item.data = createDownload({filePath: 'unique2', hideDate: false});
    await microtasksFinished();
    item.data = createDownload({hideDate: false});
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique2');
    await microtasksFinished();
    assertTrue(item.getFileIcon().hidden);
  });

  test('icon overridden by display type', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kSensitiveContentBlock,
    });
    await microtasksFinished();
    assertEquals('cr:error', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'red',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      isInsecure: true,
    });
    await microtasksFinished();

    assertEquals('cr:warning', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'grey',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kNoSafeBrowsing,
    });
    await microtasksFinished();

    assertEquals('cr:warning', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'grey',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kEnhancedProtection,
      hasSafeBrowsingVerdict: true,
    });
    await microtasksFinished();

    assertEquals('cr:warning', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'grey',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kStandardProtection,
      hasSafeBrowsingVerdict: false,
    });
    await microtasksFinished();

    assertEquals('cr:warning', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'grey',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDeepScannedFailed,
    });
    await microtasksFinished();

    assertEquals('cr:warning', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'grey',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousUrl,
    });
    await microtasksFinished();

    assertEquals(
        'downloads:dangerous', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'red',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kCookieTheft,
    });
    await microtasksFinished();

    assertEquals(
        'downloads:dangerous', item.shadowRoot.querySelector('cr-icon')!.icon);
    assertTrue(item.$.fileIcon.hidden);
    assertEquals(
        'red',
        item.shadowRoot.querySelector('cr-icon')!.getAttribute('icon-color'));
  });

  test('description color set by display type', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kSensitiveContentBlock,
    });
    await microtasksFinished();

    assertEquals(
        'red',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      isInsecure: true,
    });
    await microtasksFinished();

    assertEquals(
        'grey',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kNoSafeBrowsing,
    });
    await microtasksFinished();

    assertEquals(
        'grey',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kEnhancedProtection,
      hasSafeBrowsingVerdict: true,
    });
    await microtasksFinished();

    assertEquals(
        'grey',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousFile,
      safeBrowsingState: SafeBrowsingState.kStandardProtection,
      hasSafeBrowsingVerdict: false,
    });
    await microtasksFinished();

    assertEquals(
        'grey',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDeepScannedFailed,
    });
    await microtasksFinished();

    assertEquals(
        'grey',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      dangerType: DangerType.kDangerousUrl,
    });
    await microtasksFinished();

    assertEquals(
        'red',
        item.shadowRoot.querySelector('.description')!.getAttribute(
            'description-color'));
  });

  test('description text overridden by tailored warning type', async () => {
    function assertDescriptionText(expected: string) {
      assertEquals(
          expected,
          item.shadowRoot.querySelector(
                             '.description:not([hidden])')!.textContent.trim());
    }

    // Suspicious archive
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      dangerType: DangerType.kUncommonContent,
      tailoredWarningType: TailoredWarningType.kSuspiciousArchive,
    });
    await microtasksFinished();
    assertDescriptionText(
        loadTimeData.getString('dangerUncommonSuspiciousArchiveDesc'));

    // Cookie theft without account
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      dangerType: DangerType.kCookieTheft,
      tailoredWarningType: TailoredWarningType.kCookieTheft,
    });
    await microtasksFinished();
    assertDescriptionText(loadTimeData.getString('dangerDownloadCookieTheft'));
  });

  test('icon aria-hidden determined by display type', async () => {
    testIconLoader.setShouldIconsLoad(true);

    const iconWrapper = item.shadowRoot.querySelector('.icon-wrapper');

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      isInsecure: true,
    });
    await microtasksFinished();
    assertTrue(!!iconWrapper);
    assertEquals('false', iconWrapper.ariaHidden);

    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      hideDate: true,
      state: State.kDangerous,
    });
    await microtasksFinished();
    assertTrue(!!iconWrapper);
    assertEquals('false', iconWrapper.ariaHidden);

    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
    });
    await microtasksFinished();
    assertTrue(!!iconWrapper);
    assertEquals('true', iconWrapper.ariaHidden);
  });

  test('save dangerous click fires event for dangerous', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      dangerType: DangerType.kDangerousUrl,
    });
    await microtasksFinished();
    const whenFired = eventToPromise('save-dangerous-click', item);
    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    const saveDangerousButton =
        item.shadowRoot.querySelector<HTMLElement>('#save-dangerous');
    assertTrue(!!saveDangerousButton);
    assertTrue(isVisible(saveDangerousButton));
    saveDangerousButton.click();
    await microtasksFinished();
    return whenFired;
  });

  test('save dangerous click does not fire event for suspicious', async () => {
    item.data = createDownload({
      id: 'itemId',
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      // Uncommon content is suspicious, not dangerous, so no dialog
      // event should be fired.
      dangerType: DangerType.kUncommonContent,
    });
    await microtasksFinished();

    // The event should never be fired.
    item.addEventListener('save-dangerous-click', () => {
      assertNotReached('Unexpected event fired');
    });

    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    const saveDangerousButton =
        item.shadowRoot.querySelector<HTMLElement>('#save-dangerous');
    assertTrue(!!saveDangerousButton);
    assertTrue(isVisible(saveDangerousButton));
    saveDangerousButton.click();
    await microtasksFinished();
    // The mojo handler is called directly, no event for the dialog is fired.
    const id = await testDownloadsProxy.handler.whenCalled(
        'saveSuspiciousRequiringGesture');
    assertEquals('itemId', id);
  });

  test('deep scan dropdown buttons shown on correct state', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kPromptForScanning,
    });
    await microtasksFinished();
    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    assertTrue(
        isVisible(item.shadowRoot.querySelector<HTMLElement>('#deep-scan')));
    assertTrue(isVisible(
        item.shadowRoot.querySelector<HTMLElement>('#bypass-deep-scan')));
  });

  test('local decryption scan icon and text', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kPromptForLocalPasswordScanning,
    });
    await microtasksFinished();

    const icon = item.shadowRoot.querySelector<CrIconElement>(
        'cr-icon[icon-color=grey]');
    assertTrue(!!icon);
    assertEquals('cr:warning', icon.icon);
    assertEquals(
        loadTimeData.getString('controlLocalPasswordScan'),
        item.shadowRoot.querySelector<HTMLElement>(
                           '#deepScan')!.textContent.trim());
  });

  test('open anyway dropdown button shown on failed deep scan', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kComplete,
      dangerType: DangerType.kDeepScannedFailed,
    });
    await microtasksFinished();
    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    assertTrue(
        isVisible(item.shadowRoot.querySelector<HTMLElement>('#open-anyway')));
  });

  test('undo is shown in toast', async () => {
    item.data = createDownload({hideDate: false});
    await microtasksFinished();
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    // There's no menu button when the item is normal (not dangerous) and
    // completed.
    const moreActionsButton = item.getMoreActionsButton();
    assertFalse(isVisible(moreActionsButton));
    const quickRemoveButton =
        item.shadowRoot.querySelector<HTMLElement>('#quick-remove');
    assertTrue(!!quickRemoveButton);
    assertTrue(isVisible(quickRemoveButton));
    quickRemoveButton.click();
    assertFalse(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });

  test('undo is not shown in toast when item is dangerous', async () => {
    item.data = createDownload({
      hideDate: false,
      isDangerous: true,
      state: State.kDangerous,
    });
    await microtasksFinished();
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    const removeButton =
        item.shadowRoot.querySelector<HTMLElement>('#discard-dangerous');
    assertTrue(!!removeButton);
    removeButton.click();
    assertTrue(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });

  test('undo is not shown in toast when item is insecure', async () => {
    item.data = createDownload({
      hideDate: false,
      isInsecure: true,
      state: State.kInsecure,
    });
    await microtasksFinished();
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    const removeButton =
        item.shadowRoot.querySelector<HTMLElement>('#discard-dangerous');
    assertTrue(!!removeButton);
    removeButton.click();
    assertTrue(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });

  test('quick remove button discards dangerous item', async function() {
    item.data = createDownload({
      id: 'itemId',
      filePath: 'unique1',
      hideDate: false,
      isDangerous: true,
      state: State.kDangerous,
    });
    await microtasksFinished();
    const quickRemoveButton =
        item.shadowRoot.querySelector<HTMLElement>('#quick-remove');
    assertTrue(!!quickRemoveButton);
    assertTrue(isVisible(quickRemoveButton));
    quickRemoveButton.click();
    await microtasksFinished();
    const id = await testDownloadsProxy.handler.whenCalled('discardDangerous');
    assertEquals('itemId', id);
  });

  test('quick remove button removes normal item', async function() {
    item.data = createDownload({
      id: 'itemId',
      filePath: 'unique1',
      hideDate: false,
    });
    await microtasksFinished();
    const quickRemoveButton =
        item.shadowRoot.querySelector<HTMLElement>('#quick-remove');
    assertTrue(!!quickRemoveButton);
    assertTrue(isVisible(quickRemoveButton));
    quickRemoveButton.click();
    await microtasksFinished();
    const id = await testDownloadsProxy.handler.whenCalled('remove');
    assertEquals('itemId', id);
  });

  test('copy download link button hidden when url not present', async () => {
    item.data = createDownload({url: undefined});
    await microtasksFinished();
    const copyDownloadLinkButton =
        item.shadowRoot.querySelector<HTMLElement>('#copy-download-link');
    assertFalse(isVisible(copyDownloadLinkButton));
  });

  // <if expr="_google_chrome">
  test('ESBDownloadRowPromoShownAndClicked', async () => {
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.showEsbPromotion = true;
    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kDangerous,
      isDangerous: true,
      url: 'http://evil.com',
    });
    await microtasksFinished();
    const esbPromo =
        item.shadowRoot.querySelector<HTMLElement>('#esb-download-row-promo');
    assertTrue(!!esbPromo);
    assertTrue(isVisible(esbPromo));
    esbPromo.click();
    await testDownloadsProxy.handler.whenCalled('openEsbSettings');
  });

  test('ESBDownloadRowPromoNotShown', async () => {
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.showEsbPromotion = false;
    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kDangerous,
      isDangerous: true,
      url: 'http://evil.com',
    });
    await microtasksFinished();
    const esbPromo = item.shadowRoot.querySelector('#esb-download-row-promo');
    assertFalse(isVisible(esbPromo));
  });
  // </if>

  test('DownloadIsActive', async () => {
    const content = item.shadowRoot.querySelector<HTMLElement>('#content');
    assertTrue(!!content);
    item.data = createDownload({
      state: State.kComplete,
    });
    await microtasksFinished();
    assertTrue(content.classList.contains('is-active'));

    item.data = createDownload({
      state: State.kInProgress,
    });
    await microtasksFinished();
    assertTrue(content.classList.contains('is-active'));

    item.data = createDownload({
      state: State.kDangerous,
    });
    await microtasksFinished();
    assertFalse(content.classList.contains('is-active'));
  });
});

suite('ItemFocusTest', function() {
  let item: DownloadsItemElement;
  let testDownloadsProxy: TestDownloadsProxy;
  let testIconLoader: TestIconLoader;
  let toastManager: CrToastManagerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testDownloadsProxy = new TestDownloadsProxy();

    BrowserProxy.setInstance(testDownloadsProxy);

    testIconLoader = new TestIconLoader();
    IconLoaderImpl.setInstance(testIconLoader);

    item = document.createElement('downloads-item');
    document.body.appendChild(item);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  // Must be an interactive test because the clipboard write call will fail
  // if the document does not have focus. This can happen in standard browser
  // tests that are async.
  test(
      'copy download link button copies download url and shows toast',
      async () => {
        const url = 'https://example.com';
        item.data = createDownload({url: url});
        await microtasksFinished();
        const copyDownloadLinkButton =
            item.shadowRoot.querySelector<HTMLElement>('#copy-download-link');
        assertTrue(!!copyDownloadLinkButton);
        copyDownloadLinkButton.click();
        const clipboardText = await navigator.clipboard.readText();

        assertTrue(toastManager.isToastOpen);
        assertTrue(toastManager.slottedHidden);
        assertEquals(clipboardText, url);
      });

  test(
      'copy download link button copies data url and shows generic toast',
      async () => {
        const url = 'data:text/plain,hello://world';
        item.data = createDownload({url: url});
        await microtasksFinished();
        const copyDownloadLinkButton =
            item.shadowRoot.querySelector<HTMLElement>('#copy-download-link');
        assertTrue(!!copyDownloadLinkButton);
        copyDownloadLinkButton.click();
        const clipboardText = await navigator.clipboard.readText();

        assertTrue(toastManager.isToastOpen);
        assertTrue(toastManager.slottedHidden);
        // The toast message should not contain the URL to prevent spoofing.
        const toastContent = (toastManager.$.content.textContent || '').trim();
        assertFalse(toastContent.includes(url));
        assertEquals(toastContent, loadTimeData.getString('toastCopiedLink'));
        assertEquals(clipboardText, url);
      });
});
