// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './file_path.mojom-lite.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'scan-done-section' shows the post-scan user options.
 */
Polymer({
  is: 'scan-done-section',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /** @type {number} */
    numFilesSaved: Number,

    /** @type {?mojoBase.mojom.FilePath} */
    lastScannedFilePath: Object,

    /** @type {string} */
    selectedFolder: String,

    /** @private {string} */
    fileSavedTextContent_: String,
  },

  observers: ['setFileSavedTextContent_(numFilesSaved, selectedFolder)'],

  /** @override */
  created() {
    // ScanningBrowserProxy is initialized when scanning_app.js is created.
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
  },

  /** @private */
  onDoneClick_() {
    this.fire('done-click');
  },

  /** @private */
  showFileInLocation_() {
    this.browserProxy_.showFileInLocation(this.lastScannedFilePath.path)
        .then(
            /* @type {boolean} */ (succesful) => {
              if (!succesful) {
                this.fire('file-not-found');
              }
            });
  },

  /** @private */
  setFileSavedTextContent_() {
    this.browserProxy_.getPluralString('fileSavedText', this.numFilesSaved)
        .then(
            /* @type {string} */ (pluralString) => {
              this.fileSavedTextContent_ =
                  this.getAriaLabelledContent_(loadTimeData.substituteString(
                      pluralString, this.selectedFolder));
              const linkElement = this.$$('#folderLink');
              linkElement.setAttribute('href', '#');
              linkElement.addEventListener(
                  'click', () => this.showFileInLocation_());
            });
  },

  /**
   * Takes a localized string that contains exactly one anchor tag and labels
   * the string contained within the anchor tag with the entire localized
   * string. The string should not be bound by element tags. The string should
   * not contain any elements other than the single anchor tagged element that
   * will be aria-labelledby the entire string.
   * @param {string} localizedString
   * @return {string}
   * @private
   */
  getAriaLabelledContent_(localizedString) {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType == Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `id${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }

      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType == Node.ELEMENT_NODE && node.nodeName == 'A') {
        ariaLabelledByIds.push(node.id);
        return;
      }
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    anchorTags[0].setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));

    return tempEl.innerHTML;
  },
});
