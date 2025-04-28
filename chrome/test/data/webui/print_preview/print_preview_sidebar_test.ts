// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement, PrintPreviewSidebarElement} from 'chrome://print/print_preview.js';
import {NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

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
    sidebar.settings = model.settings;
    sidebar.setSetting('duplex', false);
    sidebar.pageCount = 1;
    fakeDataBind(model, sidebar, 'settings');
    document.body.appendChild(sidebar);
    sidebar.init(false, 'FooDevice', null, false);

    return nativeLayer.whenCalled('getPrinterCapabilities');
  });

  test(
      'SettingsSectionsVisibilityChange', function() {
        const moreSettingsElement =
            sidebar.shadowRoot!.querySelector('print-preview-more-settings')!;
        moreSettingsElement.$.label.click();
        function camelToKebab(s: string): string {
          return s.replace(/([A-Z])/g, '-$1').toLowerCase();
        }

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
});
