// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, CrToastManagerElement, DangerType, DownloadsItemElement, IconLoaderImpl, loadTimeData, SafeBrowsingState, State} from 'chrome://downloads/downloads.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createDownload, TestDownloadsProxy, TestIconLoader} from './test_support.js';

suite('item tests', function() {
  let item: DownloadsItemElement;
  let testIconLoader: TestIconLoader;
  let toastManager: CrToastManagerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // This isn't strictly necessary, but is a probably good idea.
    BrowserProxy.setInstance(new TestDownloadsProxy());

    testIconLoader = new TestIconLoader();
    IconLoaderImpl.setInstance(testIconLoader);

    item = document.createElement('downloads-item');
    document.body.appendChild(item);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('dangerous downloads aren\'t linkable', () => {
    item.set('data', createDownload({
               dangerType: DangerType.kDangerousFile,
               fileExternallyRemoved: false,
               hideDate: true,
               state: State.kDangerous,
               url: stringToMojoUrl('http://evil.com'),
             }));
    flush();

    assertFalse(isVisible(item.$['file-link']));
    assertFalse(item.$.url.hasAttribute('href'));
    assertFalse(item.$['file-link'].hasAttribute('href'));
  });

  test('downloads without original url in data aren\'t linkable', () => {
    const displayUrl = 'https://test.test';
    item.set('data', createDownload({
               hideDate: false,
               state: State.kComplete,
               url: undefined,
               displayUrl: stringToMojoString16(displayUrl),
             }));
    flush();

    assertFalse(item.$.url.hasAttribute('href'));
    assertFalse(item.$['file-link'].hasAttribute('href'));
    assertEquals(displayUrl, item.$.url.text);
  });

  test('failed deep scans aren\'t linkable', () => {
    item.set('data', createDownload({
               dangerType: DangerType.kDeepScannedFailed,
               fileExternallyRemoved: false,
               hideDate: true,
               state: State.kComplete,
               url: stringToMojoUrl('http://evil.com'),
             }));
    flush();

    assertFalse(isVisible(item.$['file-link']));
    assertFalse(item.$.url.hasAttribute('href'));
  });

  test('url display string is a link to the original url', () => {
    const url = 'https://' +
        'a'.repeat(1000) + '.com/document.pdf';
    const displayUrl = 'https://' +
        '啊'.repeat(1000) + '.com/document.pdf';
    item.set('data', createDownload({
               hideDate: false,
               state: State.kComplete,
               url: stringToMojoUrl(url),
               displayUrl: stringToMojoString16(displayUrl),
             }));
    flush();

    assertEquals(url, item.$.url.href);
    assertEquals(url, item.$['file-link'].href);
    assertEquals(displayUrl, item.$.url.text);
  });

  test('icon loads successfully', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.set('data', createDownload({filePath: 'unique1', hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique1');
    flush();
    assertFalse(item.getFileIcon().hidden);
  });

  test('icon fails to load', async () => {
    testIconLoader.setShouldIconsLoad(false);
    item.set('data', createDownload({filePath: 'unique2', hideDate: false}));
    item.set('data', createDownload({hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique2');
    flush();
    assertTrue(item.getFileIcon().hidden);
  });

  test('icon overridden by danger type', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.kSensitiveContentBlock,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);

    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.kBlockedTooLarge,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);

    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.kBlockedPasswordProtected,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);

    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.kDeepScannedFailed,
             }));

    assertEquals('cr:info', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);
  });

  test(
      'icon overridden by display type for improvedDownloadWarningsUX',
      async () => {
        loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
        const item = document.createElement('downloads-item');
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        document.body.appendChild(item);
        testIconLoader.setShouldIconsLoad(true);
        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kSensitiveContentBlock,
                 }));

        assertEquals(
            'cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'red',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   isInsecure: true,
                 }));

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kNoSafeBrowsing,
                 }));

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kEnhancedProtection,
                   hasSafeBrowsingVerdict: true,
                 }));

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kStandardProtection,
                   hasSafeBrowsingVerdict: false,
                 }));

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDeepScannedFailed,
                 }));

        assertEquals(
            'cr:warning', item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousUrl,
                 }));

        assertEquals(
            'downloads:dangerous',
            item.shadowRoot!.querySelector('iron-icon')!.icon);
        assertTrue(item.$['file-icon'].hidden);
        assertEquals(
            'red',
            item.shadowRoot!.querySelector('iron-icon')!.getAttribute(
                'icon-color'));
      });

  test(
      'description color set by display type for improvedDownloadWarningsUX',
      async () => {
        loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
        const item = document.createElement('downloads-item');
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        document.body.appendChild(item);

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kSensitiveContentBlock,
                 }));

        assertEquals(
            'red',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   isInsecure: true,
                 }));

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kNoSafeBrowsing,
                 }));

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kEnhancedProtection,
                   hasSafeBrowsingVerdict: true,
                 }));

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousFile,
                   safeBrowsingState: SafeBrowsingState.kStandardProtection,
                   hasSafeBrowsingVerdict: false,
                 }));

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDeepScannedFailed,
                 }));

        assertEquals(
            'grey',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));

        item.set('data', createDownload({
                   filePath: 'unique1',
                   hideDate: false,
                   dangerType: DangerType.kDangerousUrl,
                 }));

        assertEquals(
            'red',
            item.shadowRoot!.querySelector('.description')!.getAttribute(
                'description-color'));
      });

  test('open now dropdown button allowed by load time data', async () => {
    loadTimeData.overrideValues({
      'allowOpenNow': true,
      'updateDeepScanningUX': false,
      'improvedDownloadWarningsUX': true,
    });
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: State.kAsyncScanning,
             }));
    flush();
    item.getMoreActionsButton().click();
    assertTrue(
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#open-now')));
  });

  test('open now dropdown button forbidden by load time data', async () => {
    loadTimeData.overrideValues({
      'allowOpenNow': false,
      'updateDeepScanningUX': false,
      'improvedDownloadWarningsUX': true,
    });
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: State.kAsyncScanning,
             }));
    flush();
    item.getMoreActionsButton().click();
    assertFalse(
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#open-now')));
  });

  test('deep scan dropdown buttons shown on correct state', () => {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: State.kPromptForScanning,
             }));
    flush();
    item.getMoreActionsButton().click();
    assertTrue(
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#deep-scan')));
    assertTrue(isVisible(
        item.shadowRoot!.querySelector<HTMLElement>('#bypass-deep-scan')));
  });

  test('open anyway dropdown button shown on failed deep scan', () => {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: State.kComplete,
               dangerType: DangerType.kDeepScannedFailed,
             }));
    flush();
    item.getMoreActionsButton().click();
    assertTrue(
        isVisible(item.shadowRoot!.querySelector<HTMLElement>('#open-anyway')));
  });

  test('undo is shown in toast', () => {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({hideDate: false}));
    flush();
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    item.getMoreActionsButton().click();
    const removeButton = item.shadowRoot!.querySelector<HTMLElement>('#remove');
    assertTrue(!!removeButton);
    removeButton!.click();
    assertFalse(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });

  test('undo is not shown in toast when item is dangerous', () => {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({hideDate: false, isDangerous: true}));
    flush();
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    item.getMoreActionsButton().click();
    const removeButton = item.shadowRoot!.querySelector<HTMLElement>('#remove');
    assertTrue(!!removeButton);
    removeButton.click();
    assertTrue(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });

  test('undo is not shown in toast when item is insecure', () => {
    loadTimeData.overrideValues({'improvedDownloadWarningsUX': true});
    const item = document.createElement('downloads-item');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(item);
    item.set('data', createDownload({hideDate: false, isInsecure: true}));
    flush();
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    item.getMoreActionsButton().click();
    const removeButton = item.shadowRoot!.querySelector<HTMLElement>('#remove');
    assertTrue(!!removeButton);
    removeButton.click();
    assertTrue(toastManager.slottedHidden);
    assertFalse(item.getMoreActionsMenu().open);
  });
});
