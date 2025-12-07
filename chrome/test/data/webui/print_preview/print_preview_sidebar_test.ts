// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewModelElement, PrintPreviewSidebarElement, Settings} from 'chrome://print/print_preview.js';
import {NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate} from './print_preview_test_utils.js';


suite('PrintPreviewSidebarTest', function() {
  let sidebar: PrintPreviewSidebarElement;
  let model: PrintPreviewModelElement;
  let nativeLayer: NativeLayerStub;

  setup(function() {
    // Stub out the native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayer.setLocalDestinationCapabilities(getCddTemplate('FooDevice'));

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    sidebar = document.createElement('print-preview-sidebar');
    sidebar.setSetting('duplex', false);
    sidebar.pageCount = 1;
    document.body.appendChild(sidebar);
    sidebar.init(false, 'FooDevice', null, false);

    return nativeLayer.whenCalled('getPrinterCapabilities');
  });

  test('SettingsSectionsVisibilityChange', async function() {
    const moreSettingsElement =
        sidebar.shadowRoot.querySelector('print-preview-more-settings')!;
    moreSettingsElement.$.label.click();

    function camelToKebab(s: string): string {
      return s.replace(/([A-Z])/g, '-$1').toLowerCase();
    }

    const keys: Array<keyof Settings> = [
      'copies',
      'layout',
      'color',
      'mediaSize',
      'margins',
      'dpi',
      'scaling',
      'duplex',
      'otherOptions',
    ];

    for (const setting of keys) {
      const element = sidebar.shadowRoot.querySelector<HTMLElement>(
          `print-preview-${camelToKebab(setting)}-settings`)!;
      // Show, hide and reset.
      for (const value of [true, false, true]) {
        model.setSettingAvailableForTesting(setting, value);
        await microtasksFinished();
        // Element expected to be visible when available.
        assertEquals(!value, element.hidden);
      }
    }
  });
});
