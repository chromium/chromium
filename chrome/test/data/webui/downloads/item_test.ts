// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrIconElement, CrToastManagerElement, DownloadsItemElement} from 'chrome://downloads/downloads.js';
import {BrowserProxy, DangerType, IconLoaderImpl, loadTimeData, SafeBrowsingState, State, TailoredWarningType} from 'chrome://downloads/downloads.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
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
      url: stringToMojoUrl('http://evil.com'),
    });
    await microtasksFinished();

    assertFalse(isVisible(item.$['file-link']));
    assertFalse(item.$.url.hasAttribute('href'));
    assertFalse(item.$['file-link'].hasAttribute('href'));
  });

  test('downloads without original url in data aren\'t linkable', async () => {
    const displayUrl = 'https://test.test';
    item.data = createDownload({
      hideDate: false,
      state: State.kComplete,
      url: undefined,
      displayUrl: stringToMojoString16(displayUrl),
    });
    await microtasksFinished();

    assertFalse(item.$.url.hasAttribute('href'));
    assertFalse(item.$['file-link'].hasAttribute('href'));
    assertEquals(displayUrl, item.$.url.text);
  });

  test('referrer url is hidden when showReferrerUrl disabled', async () => {
    loadTimeData.overrideValues({showReferrerUrl: false});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.data = createDownload({
      hideDate: false,
      state: State.kComplete,
      referrerUrl: stringToMojoUrl('http://test.com'),
    });
    await microtasksFinished();

    assertTrue(isVisible(item.$.url));
    assertFalse(isVisible(item.$['referrer-url']));
  });

  test(
      'referrer url on downloads without referrer url in data isn\'t displayed',
      async () => {
        loadTimeData.overrideValues({showReferrerUrl: true});
        const item = document.createElement('downloads-item');
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        document.body.appendChild(item);
        item.data = createDownload({
          hideDate: false,
          state: State.kComplete,
          referrerUrl: undefined,
          displayReferrerUrl: stringToMojoString16(''),
        });
        await microtasksFinished();

        assertFalse(isVisible(item.$.url));
        assertFalse(isVisible(item.$['referrer-url']));
        assertEquals(null, item.getReferrerUrlAnchorElement());
      });

  test('referrer url on dangerous downloads isn\'t linkable', async () => {
    const referrerUrl = 'https://test.com';
    const displayReferrerUrl = 'https://displaytest.com';
    item.data = createDownload({
      dangerType: DangerType.kDangerousFile,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kDangerous,
      referrerUrl: stringToMojoUrl(referrerUrl),
      displayReferrerUrl: stringToMojoString16(displayReferrerUrl),
    });
    await microtasksFinished();

    const referrerUrlLink = item.getReferrerUrlAnchorElement();
    assertTrue(!!referrerUrlLink);
    assertTrue(isVisible(referrerUrlLink));
    assertFalse(referrerUrlLink.hasAttribute('href'));
    assertEquals(displayReferrerUrl, referrerUrlLink.text);
  });

  test(
      'referrer url display string is a link to the referrer url', async () => {
        const url = 'https://' +
            'b'.repeat(1000) + '.com/document.pdf';
        const referrerUrl = 'https://' +
            'a'.repeat(1000) + '.com/document.pdf';
        const displayReferrerUrl = 'https://' +
            '啊'.repeat(1000) + '.com/document.pdf';
        item.data = createDownload({
          hideDate: false,
          state: State.kComplete,
          url: stringToMojoUrl(url),
          referrerUrl: stringToMojoUrl(referrerUrl),
          displayReferrerUrl: stringToMojoString16(displayReferrerUrl),
        });
        await microtasksFinished();

        assertEquals(url, item.$.url.href);
        const referrerUrlLink = item.getReferrerUrlAnchorElement();
        assertTrue(!!referrerUrlLink);
        assertEquals(referrerUrl, referrerUrlLink.href);
        assertEquals(displayReferrerUrl, referrerUrlLink.text);
      });

  test('failed deep scans aren\'t linkable', async () => {
    loadTimeData.overrideValues({showReferrerUrl: true});
    item.data = createDownload({
      dangerType: DangerType.kDeepScannedFailed,
      fileExternallyRemoved: false,
      hideDate: true,
      state: State.kComplete,
      url: stringToMojoUrl('http://evil.com'),
      referrerUrl: stringToMojoUrl('http://referrer.com'),
      displayReferrerUrl: stringToMojoString16('http://display.com'),
    });
    await microtasksFinished();

    assertFalse(isVisible(item.$['file-link']));
    assertFalse(item.$.url.hasAttribute('href'));
    const referrerUrlLink = item.getReferrerUrlAnchorElement();
    assertTrue(!!referrerUrlLink);
    assertFalse(referrerUrlLink.hasAttribute('href'));
  });

  test('url display string is a link to the original url', async () => {
    const url = 'https://' +
        'a'.repeat(1000) + '.com/document.pdf';
    const displayUrl = 'https://' +
        '啊'.repeat(1000) + '.com/document.pdf';
    item.data = createDownload({
      hideDate: false,
      state: State.kComplete,
      url: stringToMojoUrl(url),
      displayUrl: stringToMojoString16(displayUrl),
    });
    await microtasksFinished();

    assertEquals(url, item.$.url.href);
    assertEquals(url, item.$['file-link'].href);
    assertEquals(displayUrl, item.$.url.text);
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

  test(
      'icon overridden by display type', async () => {
        testIconLoader.setShouldIconsLoad(true);
        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kSensitiveContentBlock,
        });
        await microtasksFinished();
        assertEquals(
            'cr:error', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'red',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          isInsecure: true,
        });
        await microtasksFinished();

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDangerousFile,
          safeBrowsingState: SafeBrowsingState.kNoSafeBrowsing,
        });
        await microtasksFinished();

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDangerousFile,
          safeBrowsingState: SafeBrowsingState.kEnhancedProtection,
          hasSafeBrowsingVerdict: true,
        });
        await microtasksFinished();

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDangerousFile,
          safeBrowsingState: SafeBrowsingState.kStandardProtection,
          hasSafeBrowsingVerdict: false,
        });
        await microtasksFinished();

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDeepScannedFailed,
        });
        await microtasksFinished();

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDangerousUrl,
        });
        await microtasksFinished();

        assertEquals(
            'downloads:dangerous',
            item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'red',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kCookieTheft,
        });
        await microtasksFinished();

        assertEquals(
            'downloads:dangerous',
            item.shadowRoot!.querySelector('cr-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'red',
            item.shadowRoot!.querySelector('cr-icon')!.getAttribute(
                'icon-color'));
      });

  test(
      'description color set by display type', async () => {
        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kSensitiveContentBlock,
        });
        await microtasksFinished();

        assertEquals(
            'red',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          isInsecure: true,
        });
        await microtasksFinished();

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
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
            item.shadowRoot!.querySelector('.description')!.getAttribute(
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
            item.shadowRoot!.querySelector('.description')!.getAttribute(
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
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDeepScannedFailed,
        });
        await microtasksFinished();

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.data = createDownload({
          filePath: 'unique1',
          hideDate: false,
          dangerType: DangerType.kDangerousUrl,
        });
        await microtasksFinished();

        assertEquals(
            'red',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));
      });

  test('description text overridden by tailored warning type', async () => {
    function assertDescriptionText(expected: string) {
      assertEquals(
          expected,
          item.shadowRoot!.querySelector('.description:not([hidden])')!
              .textContent!.trim());
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

    // Cookie theft with account
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      dangerType: DangerType.kCookieTheft,
      tailoredWarningType: TailoredWarningType.kCookieTheftWithAccountInfo,
      accountEmail: 'alice@gmail.com',
    });
    await microtasksFinished();
    assertDescriptionText(loadTimeData.getStringF(
        'dangerDownloadCookieTheftAndAccountDesc', 'alice@gmail.com'));

    // Cookie theft with empty account
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kDangerous,
      dangerType: DangerType.kCookieTheft,
      tailoredWarningType: TailoredWarningType.kCookieTheftWithAccountInfo,
    });
    await microtasksFinished();
    assertDescriptionText(loadTimeData.getString('dangerDownloadCookieTheft'));
  });

  test('icon aria-hidden determined by display type', async () => {
    testIconLoader.setShouldIconsLoad(true);

    const iconWrapper = item.shadowRoot!.querySelector('.icon-wrapper');

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
    assertEquals('true', iconWrapper!.ariaHidden);
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
        item.shadowRoot!.querySelector<HTMLElement>('#save-dangerous');
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
    item.addEventListener('save-dangerous-click', async () => {
      assertNotReached('Unexpected event fired');
    });

    const moreActionsButton = item.getMoreActionsButton();
    assertTrue(!!moreActionsButton);
    assertTrue(isVisible(moreActionsButton));
    moreActionsButton.click();
    const saveDangerousButton =
        item.shadowRoot!.querySelector<HTMLElement>('#save-dangerous');
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
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#deep-scan')));
    assertTrue(isVisible(
        item.shadowRoot!.querySelector<HTMLElement>('#bypass-deep-scan')));
  });

  test('local decryption scan icon and text', async () => {
    item.data = createDownload({
      filePath: 'unique1',
      hideDate: false,
      state: State.kPromptForLocalPasswordScanning,
    });
    await microtasksFinished();

    const icon = item.shadowRoot!.querySelector<CrIconElement>(
        'cr-icon[icon-color=grey]');
    assertTrue(!!icon);
    assertEquals('cr:warning', icon.icon);
    assertEquals(
        loadTimeData.getString('controlLocalPasswordScan'),
        item.shadowRoot!.querySelector<HTMLElement>(
                            '#deepScan')!.textContent!.trim());
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
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#open-anyway')));
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
        item.shadowRoot!.querySelector<HTMLElement>('#quick-remove');
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
        item.shadowRoot!.querySelector<HTMLElement>('#discard-dangerous');
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
        item.shadowRoot!.querySelector<HTMLElement>('#discard-dangerous');
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
        item.shadowRoot!.querySelector<HTMLElement>('#quick-remove');
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
        item.shadowRoot!.querySelector<HTMLElement>('#quick-remove');
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
        item.shadowRoot!.querySelector<HTMLElement>('#copy-download-link');
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
      url: stringToMojoUrl('http://evil.com'),
    });
    await microtasksFinished();
    const esbPromo =
        item.shadowRoot!.querySelector<HTMLElement>('#esb-download-row-promo');
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
      url: stringToMojoUrl('http://evil.com'),
    });
    await microtasksFinished();
    const esbPromo = item.shadowRoot!.querySelector('#esb-download-row-promo');
    assertFalse(isVisible(esbPromo));
  });
  // </if>

  test('DownloadIsActive', async () => {
    const content = item.shadowRoot!.querySelector<HTMLElement>('#content');
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
        item.data = createDownload({url: stringToMojoUrl(url)});
        await microtasksFinished();
        const copyDownloadLinkButton =
            item.shadowRoot!.querySelector<HTMLElement>('#copy-download-link');
        assertTrue(!!copyDownloadLinkButton);
        copyDownloadLinkButton.click();
        const clipboardText = await navigator.clipboard.readText();

        assertTrue(toastManager.isToastOpen);
        assertTrue(toastManager.slottedHidden);
        assertEquals(clipboardText, url);
      });
});
