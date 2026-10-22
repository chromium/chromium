// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement, PrintPreviewSidebarElement} from 'chrome://print/print_preview.js';
import {NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate, toggleMoreSettings} from './print_preview_test_utils.js';


suite('PrintPreviewSidebarTest', function() {
  let sidebar: PrintPreviewSidebarElement;

  let model: PrintPreviewModelElement;

  let nativeLayer: NativeLayerStub;

  setup(function() {
    // Stub out the native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    setNativeLayerCrosInstance();
    nativeLayer.setLocalDestinationCapabilities(getCddTemplate('FooDevice'));

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    sidebar = document.createElement('print-preview-sidebar');
    sidebar.settings = model.settings;
    sidebar.documentSettings = {
      allPagesHaveCustomSize: false,
      allPagesHaveCustomOrientation: false,
      hasSelection: false,
      isFromArc: false,
      isModifiable: true,
      isScalingDisabled: false,
      fitToPageScaling: 100,
      pageCount: 1,
      title: 'title',
    };
    sidebar.setSetting('duplex', false);
    sidebar.pageCount = 1;
    fakeDataBind(model, sidebar, 'settings');
    document.body.appendChild(sidebar);
    sidebar.init(false, 'FooDevice', null, false, true);

    return nativeLayer.whenCalled('getPrinterCapabilities');
  });

  function camelToKebab(s: string): string {
    return s.replace(/([A-Z])/g, '-$1').toLowerCase();
  }

  test(
      'SettingAvailabilityChangesSettingVisibility', function() {
        toggleMoreSettings(sidebar);

        ['copies', 'layout', 'color', 'mediaSize', 'margins', 'dpi', 'scaling',
         'duplex', 'otherOptions']
            .forEach(setting => {
              const element = sidebar.shadowRoot!.querySelector<HTMLElement>(
                  `print-preview-${camelToKebab(setting)}-settings`)!;
              // Show, hide and reset.
              [true, false, true].forEach(value => {
                sidebar.set(`settings.${setting}.available`, value);
                // Element expected to be visible when available.
                assertEquals(!value, element.hidden);
              });
            });
      });

  test('ManagedPrintOptionsAppliedChangesSettingVisibility', function() {
    toggleMoreSettings(sidebar);

    ['layout', 'color', 'mediaSize', 'mediaType', 'dpi', 'duplex'].forEach(
        setting => {
          const element = sidebar.shadowRoot!.querySelector<HTMLElement>(
              `print-preview-${camelToKebab(setting)}-settings`)!;
          // Disable setting availability, otherwise this setting will never be
          // hidden.
          sidebar.set(`settings.${setting}.available`, false);
          // Show, hide and reset.
          [true, false, true].forEach(value => {
            sidebar.set(
                `destination.allowedManagedPrintOptionsApplied.${setting}`,
                value);
            // Element expected to be visible when managed print options are
            // applied.
            assertEquals(!value, element.hidden);
          });
        });

    const layoutElement = sidebar.shadowRoot!.querySelector<HTMLElement>(
        'print-preview-layout-settings')!;
    sidebar.set('settings.layout.available', false);
    sidebar.set('destination.allowedManagedPrintOptionsApplied.layout', true);
    assertFalse(layoutElement.hidden);

    // PDF documents (unmodifiable and not from ARC) should hide layout even
    // when managed print options are applied.
    sidebar.set('documentSettings.isModifiable', false);
    assertTrue(layoutElement.hidden);

    // ARC documents should still show layout.
    sidebar.set('documentSettings.isFromArc', true);
    assertFalse(layoutElement.hidden);

    // HTML documents with custom CSS orientation should hide layout.
    sidebar.set('documentSettings.isModifiable', true);
    sidebar.set('documentSettings.isFromArc', false);
    sidebar.set('documentSettings.allPagesHaveCustomOrientation', true);
    assertTrue(layoutElement.hidden);
  });

  // Tests that number of sheets is correctly calculated if duplex setting is
  // enabled.
  test('SheetCountWithDuplex', function() {
    const header = sidebar.shadowRoot!.querySelector('print-preview-header')!;
    assertEquals(1, header.sheetCount);
    sidebar.setSetting('pages', [1, 2, 3]);
    assertEquals(3, header.sheetCount);
    sidebar.setSetting('duplex', true);
    assertEquals(2, header.sheetCount);
    sidebar.setSetting('pages', [1, 2]);
    assertEquals(1, header.sheetCount);
  });

  // Tests that number of sheets is correctly calculated if multiple copies
  // setting is enabled.
  test('SheetCountWithCopies', function() {
    const header = sidebar.shadowRoot!.querySelector('print-preview-header')!;
    assertEquals(1, header.sheetCount);
    sidebar.setSetting('copies', 4);
    assertEquals(4, header.sheetCount);
    sidebar.setSetting('duplex', true);
    assertEquals(4, header.sheetCount);
    sidebar.setSetting('pages', [1, 2]);
    assertEquals(4, header.sheetCount);
    sidebar.setSetting('duplex', false);
    assertEquals(8, header.sheetCount);
  });
});
