// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/scan_preview.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AccessibilityFeaturesInterface, ForceHiddenElementsVisibleObserverRemote} from 'chrome://scanning/accessibility_features.mojom-webui.js';
import {setAccessibilityFeaturesForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import type {ScanPreviewElement} from 'chrome://scanning/scan_preview.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMediaQueryList} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

class FakeAccessibilityFeatures implements AccessibilityFeaturesInterface {
  forceHiddenElementsVisibleObserverRemote:
      ForceHiddenElementsVisibleObserverRemote|null = null;

  observeForceHiddenElementsVisible(
      remote: ForceHiddenElementsVisibleObserverRemote):
      Promise<{forceVisible: boolean}> {
    return new Promise(resolve => {
      this.forceHiddenElementsVisibleObserverRemote = remote;
      resolve({forceVisible: false});
    });
  }

  simulateObserverChange(forceVisible: boolean): Promise<void> {
    assert(this.forceHiddenElementsVisibleObserverRemote);
    this.forceHiddenElementsVisibleObserverRemote
        .onForceHiddenElementsVisibleChange(forceVisible);
    return flushTasks();
  }
}

suite('scanPreviewTest', function() {
  const testSvgPath =
      'chrome://webui-test/chromeos/scanning/fake_scanned_image.svg';

  let scanPreview: ScanPreviewElement|null = null;

  let fakeAccessibilityFeatures: FakeAccessibilityFeatures|null = null;

  let helpOrProgress: HTMLElement;
  let helperText: HTMLElement;
  let scanProgress: HTMLElement;
  let scannedImages: HTMLElement;
  let cancelingProgress: HTMLElement;

  let mockController: MockController;

  let fakePrefersColorSchemeDarkMediaQuery: FakeMediaQueryList;

  function setFakePrefersColorSchemeDark(enabled: boolean): Promise<void> {
    assert(scanPreview);
    fakePrefersColorSchemeDarkMediaQuery.matches = enabled;

    return flushTasks();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    fakeAccessibilityFeatures = new FakeAccessibilityFeatures();
    setAccessibilityFeaturesForTesting(fakeAccessibilityFeatures);
    scanPreview = document.createElement('scan-preview');
    assertTrue(!!scanPreview);
    ScanningBrowserProxyImpl.setInstance(new TestScanningBrowserProxy());

    // Setup mock for matchMedia.
    mockController = new MockController();
    const mockMatchMedia =
        mockController.createFunctionMock(window, 'matchMedia');
    fakePrefersColorSchemeDarkMediaQuery =
        new FakeMediaQueryList('(prefers-color-scheme: dark)');
    mockMatchMedia.returnValue = fakePrefersColorSchemeDarkMediaQuery;

    document.body.appendChild(scanPreview);

    helpOrProgress =
        strictQuery('#helpOrProgress', scanPreview.shadowRoot, HTMLElement);
    helperText =
        strictQuery('#helperText', scanPreview.shadowRoot, HTMLElement);
    scanProgress =
        strictQuery('#scanProgress', scanPreview.shadowRoot, HTMLElement);
    scannedImages =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    cancelingProgress =
        strictQuery('#cancelingProgress', scanPreview.shadowRoot, HTMLElement);
  });

  teardown(() => {
    scanPreview?.remove();
    mockController.reset();
    scanPreview = null;
  });

  function assertVisible(
      isHelpOrProgressVisible: boolean, isHelperTextVisible: boolean,
      isScanProgressVisible: boolean, isScannedImagesVisible: boolean,
      isCancelingProgressVisible: boolean): void {
    assertEquals(isHelpOrProgressVisible, isVisible(helpOrProgress));
    assertEquals(isHelperTextVisible, isVisible(helperText));
    assertEquals(isScanProgressVisible, isVisible(scanProgress));
    assertEquals(isScannedImagesVisible, isVisible(scannedImages));
    assertEquals(isCancelingProgressVisible, isVisible(cancelingProgress));
  }

  test('initializeScanPreview', () => {
    assert(scanPreview);
    assertTrue(!!strictQuery('.preview', scanPreview.shadowRoot, HTMLElement));
  });

  // Test that the progress text updates when the page number increases.
  test('progressTextUpdates', () => {
    assert(scanPreview);
    scanPreview.appState = AppState.SCANNING;
    scanPreview.setPageNumberForTesting(1);
    const progressText =
        strictQuery('#progressText', scanPreview.shadowRoot, HTMLElement);
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 1),
        progressText.textContent!.trim());
    scanPreview.setPageNumberForTesting(2);
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 2),
        progressText.textContent!.trim());
  });

  // Tests that the correct element is showing in the preview pane depending on
  // current app state.
  test('appStateTransitions', () => {
    assert(scanPreview);
    scanPreview.appState = AppState.GETTING_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GOT_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GETTING_CAPS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.READY;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.SCANNING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ true, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.CANCELING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ true);

    scanPreview.appState = AppState.MULTI_PAGE_CANCELING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ true);

    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ false, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ true,
        /*isCancelingProgressVisible*/ false);
  });

  // Tests that the action toolbar is only displayed for multi-page scans.
  test('showActionToolbarForMultiPageScans', async () => {
    assert(scanPreview);
    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;

    await flushTasks();

    assertTrue(
        strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
            .hidden);
    scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
    flush();
    assertFalse(
        strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
            .hidden);
  });

  // Tests that the toolbar will get repositioned after subsequent scans.
  test('positionActionToolbarOnSubsequentScans', async () => {
    assert(scanPreview);
    const scannedImagesDiv =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    scanPreview.objectUrls = [];
    scanPreview.setIsMultiPageScanForTesting(true);
    scanPreview.appState = AppState.MULTI_PAGE_SCANNING;

    await flushTasks();

    // Before the image loads we expect the CSS variables to be unset.
    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

    scanPreview.objectUrls = [testSvgPath];
    scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;

    await waitAfterNextRender(scannedImagesDiv);

    // After the image loads we expect the CSS variables to be set.
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

    // Reset the CSS variabls and delete the image to simulate starting a
    // new scan.
    scanPreview.style.setProperty('--action-toolbar-top', '');
    scanPreview.style.setProperty('--action-toolbar-left', '');
    scanPreview.objectUrls = [];
    scanPreview.appState = AppState.MULTI_PAGE_SCANNING;

    await waitAfterNextRender(scannedImagesDiv);

    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

    scanPreview.objectUrls = [testSvgPath];
    scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;

    await waitAfterNextRender(scannedImagesDiv);

    // We expect the CSS variables to be set again.
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));
  });

  // Tests that the remove page dialog opens, shows the correct page number,
  // then fires the correct event when the action button is clicked.
  test('removePageDialog', async () => {
    assert(scanPreview);
    const pageIndexToRemove = 5;
    let pageIndexFromEvent;
    scanPreview.addEventListener('remove-page', (e) => {
      pageIndexFromEvent = e.detail;
    });
    scanPreview.objectUrls = [testSvgPath];

    await flushTasks();

    assertFalse(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);
    strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
        .dispatchEvent(new CustomEvent(
            'show-remove-page-dialog', {detail: pageIndexToRemove}));

    await flushTasks();

    assertTrue(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);
    assertEquals(
        'Remove page?',
        strictQuery('#dialogTitle', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertEquals(
        'Remove',
        strictQuery('#actionButton', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertEquals(
        loadTimeData.getStringF(
            'removePageConfirmationText', pageIndexToRemove + 1),
        strictQuery(
            '#dialogConfirmationText', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());

    strictQuery('#actionButton', scanPreview.shadowRoot, HTMLElement).click();
    assertFalse(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);
    assertEquals(pageIndexToRemove, pageIndexFromEvent);
  });

  // Tests that clicking the cancel button closes the remove page dialog.
  test('cancelRemovePageDialog', async () => {
    assert(scanPreview);
    scanPreview.objectUrls = ['image'];
    assertFalse(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);

    await flushTasks();

    strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
        .dispatchEvent(new CustomEvent('show-remove-page-dialog'));

    await flushTasks();

    assertTrue(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);

    strictQuery('#cancelButton', scanPreview.shadowRoot, HTMLElement).click();
    assertFalse(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);
  });

  // Tests that the rescan page dialog opens and shows the correct page number.
  test('rescanPageDialog', async () => {
    assert(scanPreview);
    const pageIndex = 6;
    scanPreview.objectUrls = ['image1', 'image2'];
    assertFalse(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);

    await flushTasks();

    strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
        .dispatchEvent(
            new CustomEvent('show-rescan-page-dialog', {detail: pageIndex}));

    await flushTasks();

    assertTrue(
        strictQuery(
            '#scanPreviewDialog', scanPreview.shadowRoot, CrDialogElement)
            .open);
    assertEquals(
        'Rescan page ' + (pageIndex + 1) + '?',
        strictQuery('#dialogTitle', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertEquals(
        loadTimeData.getStringF('rescanPageConfirmationText', pageIndex),
        strictQuery(
            '#dialogConfirmationText', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());
  });

  // Tests that for multi-page scans, resizing the app window triggers the
  // repositioning of the action toolbar.
  test('resizingWindowRepositionsActionToolbar', async () => {
    assert(scanPreview);
    const scannedImagesDiv =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    scanPreview.objectUrls = ['image'];
    scanPreview.setIsMultiPageScanForTesting(true);
    scanPreview.appState = AppState.MULTI_PAGE_SCANNING;

    await flushTasks();

    scanPreview.objectUrls = [testSvgPath];
    scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;

    await waitAfterNextRender(scannedImagesDiv);

    // After the image loads we expect the CSS variables to be set.
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

    // Reset the CSS variables and simulate the window being resized.
    scanPreview.style.setProperty('--action-toolbar-top', '');
    scanPreview.style.setProperty('--action-toolbar-left', '');
    window.dispatchEvent(new CustomEvent('resize'));

    await flushTasks();

    // The CSS variables should get set again.
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertNotEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

    // Now test that unchecking the multi-page scan checkbox removes the
    // window listener.
    scanPreview.objectUrls = [];
    scanPreview.setIsMultiPageScanForTesting(false);
    scanPreview.appState = AppState.SCANNING;

    await flushTasks();

    scanPreview.objectUrls = [testSvgPath];
    scanPreview.appState = AppState.DONE;

    // Reset the CSS variables and simulate starting the window being
    // resized.
    scanPreview.style.setProperty('--action-toolbar-top', '');
    scanPreview.style.setProperty('--action-toolbar-left', '');
    window.dispatchEvent(new CustomEvent('resize'));

    await flushTasks();

    // The CSS variables should not get set for non-multi-page scans.
    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
    assertEquals(
        '', scanPreview.style.getPropertyValue('--action-toolbar-left'));
  });

  // Tests that the action toolbar will be forced visible when the accessibility
  // features are enabled.
  test('showActionToolbarWhenAccessibilityEnabled', async () => {
    assert(scanPreview);
    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;

    await flushTasks();

    scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
    flush();
    const actionToolbar =
        strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement);
    assertEquals(
        'hidden',
        getComputedStyle(actionToolbar).getPropertyValue('visibility'));

    await fakeAccessibilityFeatures!.simulateObserverChange(
        /*forceVisible=*/ true);

    assertEquals(
        'visible',
        getComputedStyle(actionToolbar).getPropertyValue('visibility'));
  });

  // Verify "ready to scan" dynamic SVG use when dynamic colors enabled.
  test('jellyColors_ReadyToScanSvg', async () => {
    assert(scanPreview);
    const dynamicSvg = `svg/illo_ready_to_scan.svg#illo_ready_to_scan`;

    const getReadyToScanVisual = (): SVGUseElement => strictQuery(
        '#readyToScanSvg > use', scanPreview!.shadowRoot, SVGUseElement);

    // Mock media query state for light mode.
    await setFakePrefersColorSchemeDark(false);
    assertEquals(dynamicSvg, getReadyToScanVisual().href.baseVal);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(dynamicSvg, getReadyToScanVisual().href.baseVal);
  });
});
