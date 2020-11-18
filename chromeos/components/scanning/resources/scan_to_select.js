// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScanningBrowserProxy, ScanningBrowserProxyImpl, SelectedPath} from './scanning_browser_proxy.js';
import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'scan-to-select' displays the chosen directory to save completed scans.
 */
Polymer({
  is: 'scan-to-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /**
     * The lowest level directory in |selectedFilePath|.
     * @private
     */
    displayText_: String,

    /** @type {string} */
    selectedFilePath: {
      type: String,
      notify: true,
    },
  },

  /** @override */
  created() {
    // Default option is 'My files'.
    this.displayText_ = this.i18n('myFilesSelectOption');

    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
  },

  /**
   * Opens the select dialog and updates the dropdown to the user's selected
   * directory.
   * @private
   */
  onSelectFolder_() {
    this.browserProxy_.requestScanToLocation().then(
        /* @type {!SelectedPath} */ (selectedPath) => {
          // When the select dialog closes, set dropdown back to |displayText_|
          // option.
          this.$.scanToSelect.selectedIndex = 0;

          const baseName = selectedPath.baseName;
          const filePath = selectedPath.filePath;
          // When the select dialog is canceled, |baseName| and |filePath| will
          // be empty.
          if (!baseName || !filePath) {
            return;
          }

          this.displayText_ = baseName;
          this.selectedFilePath = filePath;
        });
  },
});
