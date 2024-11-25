// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {LoadTimeDataRaw} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {HIDDEN_CLASS} from './constants.js';
import {Runner} from './dino_game/offline.js';

declare global {
  interface Window {
    errorPageController?: ErrorPageController;
    loadTimeDataRaw: LoadTimeDataRaw;
  }
}

interface ErrorPageController {
  downloadButtonClick(): void;
  reloadButtonClick(url?: string): void;
  detailsButtonClick(): void;
  diagnoseErrorsButtonClick(): void;
  portalSigninButtonClick(): void;
  savePageForLater(): void;
  cancelSavePage(): void;
}

interface WithDetailsText {
  detailsText: string;
  hideDetailsText: string;
}

function toggleHelpBox() {
  const wrapper = getRequiredElement('main-frame-error');
  wrapper.classList.toggle('showing-details');

  const detailsButton =
      getRequiredElement<HTMLElement&WithDetailsText>('details-button');
  if (!wrapper.classList.contains('showing-details')) {
    detailsButton.innerText = detailsButton.detailsText;
  } else {
    detailsButton.innerText = detailsButton.hideDetailsText;
  }
}

function diagnoseErrors() {
  if (window.errorPageController) {
    window.errorPageController.diagnoseErrorsButtonClick();
  }
}

function portalSignin() {
  if (window.errorPageController) {
    window.errorPageController.portalSigninButtonClick();
  }
}

// Subframes use a different layout but the same html file.  This is to make it
// easier to support platforms that load the error page via different
// mechanisms (Currently just iOS).
let isSubFrame = false;
if (window.top!.location !== window.location) {
  document.documentElement.setAttribute('subframe', '');
  isSubFrame = true;
}

// Re-renders the error page using |strings| as the dictionary of values.
// Used by NetErrorTabHelper to update DNS error pages with probe results.
function updateForDnsProbe(strings: any) {
  const context = new JsEvalContext(strings);
  jstProcess(context, document.body);
  onDocumentLoadOrUpdate();
}

// Adds an icon class to the list and removes classes previously set.
function updateIconClass(newClass: string) {
  const frameSelector = isSubFrame ? '#sub-frame-error' : '#main-frame-error';
  const iconEl = document.querySelector(frameSelector + ' .icon');
  assert(iconEl);

  if (iconEl.classList.contains(newClass)) {
    return;
  }

  iconEl.className = 'icon ' + newClass;
}

// Implements button clicks.  This function is needed during the transition
// between implementing these in trunk chromium and implementing them in iOS.
function reloadButtonClick(url: string) {
  if (window.errorPageController) {
    // <if expr="is_ios">
    window.errorPageController.reloadButtonClick(url);
    // </if>

    // <if expr="not is_ios">
    window.errorPageController.reloadButtonClick();
    // </if>
  } else {
    window.location.href = url;
  }
}

interface WithDisabledText {
  disabledText: string;
}

function downloadButtonClick() {
  if (window.errorPageController) {
    window.errorPageController.downloadButtonClick();
    const downloadButton =
        getRequiredElement<HTMLButtonElement&WithDisabledText>(
            'download-button');
    downloadButton.disabled = true;
    downloadButton.textContent = downloadButton.disabledText;
  }
}

function detailsButtonClick() {
  if (window.errorPageController) {
    window.errorPageController.detailsButtonClick();
  }
}

let primaryControlOnLeft = true;
// clang-format off
// <if expr="is_macosx or is_ios or is_linux or is_chromeos or is_android">
// clang-format on
primaryControlOnLeft = false;
// </if>

function setAutoFetchState(scheduled: boolean, canSchedule: boolean) {
  getRequiredElement('cancel-save-page-button')
      .classList.toggle(HIDDEN_CLASS, !scheduled);
  getRequiredElement('save-page-for-later-button')
      .classList.toggle(HIDDEN_CLASS, scheduled || !canSchedule);
}

function savePageLaterClick() {
  assert(window.errorPageController);
  window.errorPageController.savePageForLater();
  // savePageForLater will eventually trigger a call to setAutoFetchState() when
  // it completes.
}

function cancelSavePageClick() {
  assert(window.errorPageController);
  window.errorPageController.cancelSavePage();
  // setAutoFetchState is not called in response to cancelSavePage(), so do it
  // now.
  setAutoFetchState(false, true);
}

// Called on document load, and from updateForDnsProbe().
function onDocumentLoadOrUpdate() {
  const downloadButtonVisible = loadTimeData.valueExists('downloadButton') &&
      loadTimeData.getValue('downloadButton').msg;
  const detailsButton = getRequiredElement('details-button');

  const reloadButtonVisible = loadTimeData.valueExists('reloadButton') &&
      loadTimeData.getValue('reloadButton').msg;

  const reloadButton = getRequiredElement('reload-button');
  const downloadButton = getRequiredElement('download-button');
  if (reloadButton.style.display === 'none' &&
      downloadButton.style.display === 'none') {
    detailsButton.classList.add('singular');
  }

  // Show or hide control buttons.
  const controlButtonDiv = getRequiredElement('control-buttons');
  controlButtonDiv.hidden = !(reloadButtonVisible || downloadButtonVisible);

  const iconClass = loadTimeData.valueExists('iconClass') &&
      loadTimeData.getValue('iconClass');

  updateIconClass(iconClass);

  if (!isSubFrame && iconClass === 'icon-offline') {
    document.documentElement.classList.add('offline');
    new Runner('.interstitial-wrapper');
  }
}

function onDocumentLoad() {
  // `loadTimeDataRaw` is injected to the `window` scope from C++.
  loadTimeData.data = window.loadTimeDataRaw;
  jstProcess(new JsEvalContext(window.loadTimeDataRaw), document.body);

  // Sets up the proper button layout for the current platform.
  const buttonsDiv = getRequiredElement('buttons');
  if (primaryControlOnLeft) {
    buttonsDiv.classList.add('suggested-left');
  } else {
    buttonsDiv.classList.add('suggested-right');
  }

  onDocumentLoadOrUpdate();
}

// Expose methods that are triggered either
//  - By `onclick=...` handlers in the HTML code, OR
//  - By `href="javascript:..."` in localized links.
//  - By inected JS code coming from C++
//
//  since those need to be available on the 'window' object.
Object.assign(window, {
  cancelSavePageClick,
  detailsButtonClick,
  diagnoseErrors,
  downloadButtonClick,
  portalSignin,
  reloadButtonClick,
  savePageLaterClick,
  toggleHelpBox,
  updateForDnsProbe,
});

document.addEventListener('DOMContentLoaded', onDocumentLoad);
