// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {mobileNav} from 'chrome://interstitials/common/resources/interstitial_mobile_nav.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {HIDDEN_CLASS} from './constants.js';
import {Runner} from './offline.js';

/**
 * @typedef {{
 *   downloadButtonClick: function(),
 *   reloadButtonClick: function(string),
 *   detailsButtonClick: function(),
 *   diagnoseErrorsButtonClick: function(),
 *   portalSigninButtonClick: function(),
 *   trackEasterEgg: function(),
 *   updateEasterEggHighScore: function(number),
 *   resetEasterEggHighScore: function(),
 *   launchOfflineItem: function(string, string),
 *   savePageForLater: function(),
 *   cancelSavePage: function(),
 *   listVisibilityChange: function(boolean),
 * }}
 */
// eslint-disable-next-line no-var
var errorPageController;

// Decodes a UTF16 string that is encoded as base64.
function decodeUTF16Base64ToString(encoded_text) {
  const data = atob(encoded_text);
  let result = '';
  for (let i = 0; i < data.length; i += 2) {
    result +=
        String.fromCharCode(data.charCodeAt(i) * 256 + data.charCodeAt(i + 1));
  }
  return result;
}

function toggleHelpBox() {
  const helpBoxOuter = document.getElementById('details');
  helpBoxOuter.classList.toggle(HIDDEN_CLASS);
  const detailsButton = document.getElementById('details-button');
  if (helpBoxOuter.classList.contains(HIDDEN_CLASS)) {
    /** @suppress {missingProperties} */
    detailsButton.innerText = detailsButton.detailsText;
  } else {
    /** @suppress {missingProperties} */
    detailsButton.innerText = detailsButton.hideDetailsText;
  }

  // Details appears over the main content on small screens.
  if (mobileNav) {
    document.getElementById('main-content').classList.toggle(HIDDEN_CLASS);
    const runnerContainer = document.querySelector('.runner-container');
    if (runnerContainer) {
      runnerContainer.classList.toggle(HIDDEN_CLASS);
    }
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
if (window.top.location !== window.location) {
  document.documentElement.setAttribute('subframe', '');
  isSubFrame = true;
}

// Re-renders the error page using |strings| as the dictionary of values.
// Used by NetErrorTabHelper to update DNS error pages with probe results.
function updateForDnsProbe(strings) {
  const context = new JsEvalContext(strings);
  jstProcess(context, document.body);
  onDocumentLoadOrUpdate();
}

// Adds an icon class to the list and removes classes previously set.
function updateIconClass(newClass) {
  const frameSelector = isSubFrame ? '#sub-frame-error' : '#main-frame-error';
  const iconEl = document.querySelector(frameSelector + ' .icon');

  if (iconEl.classList.contains(newClass)) {
    return;
  }

  iconEl.className = 'icon ' + newClass;
}

// Implements button clicks.  This function is needed during the transition
// between implementing these in trunk chromium and implementing them in iOS.
function reloadButtonClick(url) {
  if (window.errorPageController) {
    // <if expr="is_ios">
    window.errorPageController.reloadButtonClick(url);
    // </if>

    // <if expr="not is_ios">
    window.errorPageController.reloadButtonClick();
    // </if>
  } else {
    window.location = url;
  }
}

function downloadButtonClick() {
  if (window.errorPageController) {
    window.errorPageController.downloadButtonClick();
    const downloadButton = document.getElementById('download-button');
    downloadButton.disabled = true;
    /** @suppress {missingProperties} */
    downloadButton.textContent = downloadButton.disabledText;

    document.getElementById('download-link-wrapper')
        .classList.add(HIDDEN_CLASS);
    document.getElementById('download-link-clicked-wrapper')
        .classList.remove(HIDDEN_CLASS);
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

function setAutoFetchState(scheduled, can_schedule) {
  document.getElementById('cancel-save-page-button')
      .classList.toggle(HIDDEN_CLASS, !scheduled);
  document.getElementById('save-page-for-later-button')
      .classList.toggle(HIDDEN_CLASS, scheduled || !can_schedule);
}

function savePageLaterClick() {
  window.errorPageController.savePageForLater();
  // savePageForLater will eventually trigger a call to setAutoFetchState() when
  // it completes.
}

function cancelSavePageClick() {
  window.errorPageController.cancelSavePage();
  // setAutoFetchState is not called in response to cancelSavePage(), so do it
  // now.
  setAutoFetchState(false, true);
}

function toggleErrorInformationPopup() {
  document.getElementById('error-information-popup-container')
      .classList.toggle(HIDDEN_CLASS);
}

function launchOfflineItem(itemID, name_space) {
  window.errorPageController.launchOfflineItem(itemID, name_space);
}

function launchDownloadsPage() {
  window.errorPageController.launchDownloadsPage();
}

function getIconForSuggestedItem(item) {
  // Note: |item.content_type| contains the enum values from
  // chrome::mojom::AvailableContentType.
  switch (item.content_type) {
    case 1:  // kVideo
      return 'image-video';
    case 2:  // kAudio
      return 'image-music-note';
    case 0:  // kPrefetchedPage
    case 3:  // kOtherPage
      return 'image-earth';
  }
  return 'image-file';
}

function getSuggestedContentDiv(item, index) {
  // Note: See AvailableContentToValue in available_offline_content_helper.cc
  // for the data contained in an |item|.
  // TODO(carlosk): Present |snippet_base64| when that content becomes
  // available.
  let thumbnail = '';
  const extraContainerClasses = [];
  // html_inline.py will try to replace src attributes with data URIs using a
  // simple regex. The following is obfuscated slightly to avoid that.
  const source = 'src';
  if (item.thumbnail_data_uri) {
    extraContainerClasses.push('suggestion-with-image');
    thumbnail = `<img ${source}="${item.thumbnail_data_uri}">`;
  } else {
    extraContainerClasses.push('suggestion-with-icon');
    const iconClass = getIconForSuggestedItem(item);
    thumbnail = `<div><img class="${iconClass}"></div>`;
  }

  let favicon = '';
  if (item.favicon_data_uri) {
    favicon = `<img ${source}="${item.favicon_data_uri}">`;
  } else {
    extraContainerClasses.push('no-favicon');
  }

  if (!item.attribution_base64) {
    extraContainerClasses.push('no-attribution');
  }

  return `
  <div class="offline-content-suggestion ${extraContainerClasses.join(' ')}"
    onclick="launchOfflineItem('${item.ID}', '${item.name_space}')">
      <div class="offline-content-suggestion-texts">
        <div id="offline-content-suggestion-title-${index}"
             class="offline-content-suggestion-title">
        </div>
        <div class="offline-content-suggestion-attribution-freshness">
          <div id="offline-content-suggestion-favicon-${index}"
               class="offline-content-suggestion-favicon">
            ${favicon}
          </div>
          <div id="offline-content-suggestion-attribution-${index}"
               class="offline-content-suggestion-attribution">
          </div>
          <div class="offline-content-suggestion-freshness">
            ${item.date_modified}
          </div>
          <div class="offline-content-suggestion-pin-spacer"></div>
          <div class="offline-content-suggestion-pin"></div>
        </div>
      </div>
      <div class="offline-content-suggestion-thumbnail">
        ${thumbnail}
      </div>
  </div>`;
}

/**
 * @typedef {{
 *   ID: string,
 *   name_space: string,
 *   title_base64: string,
 *   snippet_base64: string,
 *   date_modified: string,
 *   attribution_base64: string,
 *   thumbnail_data_uri: string,
 *   favicon_data_uri: string,
 *   content_type: number,
 * }}
 */
let AvailableOfflineContent;

// Populates a list of suggested offline content.
// Note: For security reasons all content downloaded from the web is considered
// unsafe and must be securely handled to be presented on the dino page. Images
// have already been safely re-encoded but textual content -- like title and
// attribution -- must be properly handled here.
// @param {boolean} isShown
// @param {Array<AvailableOfflineContent>} suggestions
function offlineContentAvailable(isShown, suggestions) {
  if (!suggestions || !loadTimeData.valueExists('offlineContentList')) {
    return;
  }

  const suggestionsHTML = [];
  for (let index = 0; index < suggestions.length; index++) {
    suggestionsHTML.push(getSuggestedContentDiv(suggestions[index], index));
  }

  document.getElementById('offline-content-suggestions').innerHTML =
      suggestionsHTML.join('\n');

  // Sets textual web content using |textContent| to make sure it's handled as
  // plain text.
  for (let index = 0; index < suggestions.length; index++) {
    document.getElementById(`offline-content-suggestion-title-${index}`)
        .textContent =
        decodeUTF16Base64ToString(suggestions[index].title_base64);
    document.getElementById(`offline-content-suggestion-attribution-${index}`)
        .textContent =
        decodeUTF16Base64ToString(suggestions[index].attribution_base64);
  }

  const contentListElement = document.getElementById('offline-content-list');
  if (document.dir === 'rtl') {
    contentListElement.classList.add('is-rtl');
  }
  contentListElement.hidden = false;
  // The list is configured as hidden by default. Show it if needed.
  if (isShown) {
    toggleOfflineContentListVisibility(false);
  }
}

function toggleOfflineContentListVisibility(updatePref) {
  if (!loadTimeData.valueExists('offlineContentList')) {
    return;
  }

  const contentListElement = document.getElementById('offline-content-list');
  const isVisible = !contentListElement.classList.toggle('list-hidden');

  if (updatePref && window.errorPageController) {
    window.errorPageController.listVisibilityChanged(isVisible);
  }
}

// Called on document load, and from updateForDnsProbe().
function onDocumentLoadOrUpdate() {
  const downloadButtonVisible = loadTimeData.valueExists('downloadButton') &&
      loadTimeData.getValue('downloadButton').msg;
  const detailsButton = document.getElementById('details-button');

  // If offline content suggestions will be visible, the usual buttons will not
  // be presented.
  const offlineContentVisible =
      loadTimeData.valueExists('suggestedOfflineContentPresentation');
  if (offlineContentVisible) {
    document.querySelector('.nav-wrapper').classList.add(HIDDEN_CLASS);
    detailsButton.classList.add(HIDDEN_CLASS);

    document.getElementById('download-link').hidden = !downloadButtonVisible;
    document.getElementById('download-links-wrapper')
        .classList.remove(HIDDEN_CLASS);
    document.getElementById('error-information-popup-container')
        .classList.add('use-popup-container', HIDDEN_CLASS);
    document.getElementById('error-information-button')
        .classList.remove(HIDDEN_CLASS);
  }

  const attemptAutoFetch = loadTimeData.valueExists('attemptAutoFetch') &&
      loadTimeData.getValue('attemptAutoFetch');

  const reloadButtonVisible = loadTimeData.valueExists('reloadButton') &&
      loadTimeData.getValue('reloadButton').msg;

  const reloadButton = document.getElementById('reload-button');
  const downloadButton = document.getElementById('download-button');
  if (reloadButton.style.display === 'none' &&
      downloadButton.style.display === 'none') {
    detailsButton.classList.add('singular');
  }

  // Show or hide control buttons.
  const controlButtonDiv = document.getElementById('control-buttons');
  controlButtonDiv.hidden =
      offlineContentVisible || !(reloadButtonVisible || downloadButtonVisible);

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
  const buttonsDiv = document.getElementById('buttons');
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
  launchDownloadsPage,
  portalSignin,
  reloadButtonClick,
  savePageLaterClick,
  toggleErrorInformationPopup,
  toggleHelpBox,
  toggleOfflineContentListVisibility,
  updateForDnsProbe,
});

document.addEventListener('DOMContentLoaded', onDocumentLoad);
