// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {LoadTimeDataRaw} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {HIDDEN_CLASS} from './constants.js';
import {Runner} from './dino_game/offline.js';

declare global {
  interface Window {
    errorPageController?: ErrorPageController;
    // `loadTimeDataRaw` is injected to the `window` scope from C++.
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

interface TemplateData {
  // Properties that exist in both error pages and chrome://dino.
  errorCode: string;
  heading: {
    msg: string,
  };
  iconClass: string;

  // Properties that exist only when there is an actual error and not when
  // visiting chrome://dino directly.
  details?: string;
  hideDetails?: string;
  suggestionsDetails?: Array<{header: string, body: string}>;
  suggestionsSummaryList?: Array<{summary: string}>;
  suggestionsSummaryListHeader?: string;

  summary?: {
    msg: string,
  };

  reloadButton?: {
    msg: string,
    reloadUrl: string,
  };

  // TODO(crbug.com/378692755): Mark as Android only.
  downloadButton?: {
    msg: string,
    disabledMsg: string,
  };

  // TODO(crbug.com/378692755): Mark as Android only.
  savePageLater?: {
    savePageMsg: string,
    cancelMsg: string,
  };
}

let showingDetails: boolean = false;
let lastData: TemplateData|null = null;

function toggleHelpBox() {
  showingDetails = !showingDetails;
  assert(lastData);
  render(getHtml(lastData, showingDetails), getRequiredElement('content'));
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

// Re-renders the error page using |data| as the dictionary of values.
// Used by NetErrorTabHelper to update DNS error pages with probe results.
function updateForDnsProbe(newData: TemplateData) {
  onTemplateDataReceived(newData);
}

function getMainFrameErrorCssClass(showingDetails: boolean): string {
  return showingDetails ? 'showing-details' : '';
}

function getMainFrameErrorIconCssClass(data: TemplateData): string {
  return isSubFrame ? '' : data.iconClass;
}

function getSubFrameErrorIconCssClass(data: TemplateData): string {
  return isSubFrame ? data.iconClass : '';
}

function shouldShowSuggestionsSummaryList(data: TemplateData): boolean {
  return !!data.suggestionsSummaryList &&
      data.suggestionsSummaryList.length > 0;
}

function getSuggestionsSummaryItemCssClass(data: TemplateData): string {
  assert(data.suggestionsSummaryList);
  return data.suggestionsSummaryList.length === 1 ? 'single-suggestion' : '';
}

// Implements button clicks.  This function is needed during the transition
// between implementing these in trunk chromium and implementing them in iOS.
function reloadButtonClick(e: Event) {
  const url = (e.target as HTMLElement).dataset['url'];
  if (window.errorPageController) {
    // <if expr="is_ios">
    window.errorPageController.reloadButtonClick(url);
    // </if>

    // <if expr="not is_ios">
    window.errorPageController.reloadButtonClick();
    // </if>
  } else {
    assert(url);
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
  toggleHelpBox();
}

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

function shouldShowControlButtons(data: TemplateData): boolean {
  const downloadButtonVisible =
      !!data.downloadButton && !!data.downloadButton.msg;
  const reloadButtonVisible = !!data.reloadButton && !!data.reloadButton.msg;
  return reloadButtonVisible || downloadButtonVisible;
}

function shouldShowDetailsButton(data: TemplateData): boolean {
  return !!data.suggestionsDetails && data.suggestionsDetails.length > 0;
}

function getDetailsButtonCssClass(data: TemplateData): string {
  return shouldShowControlButtons(data) ? '' : 'singular';
}

function getDetailsButtonText(
    data: TemplateData, showingDetails: boolean): string {
  assert(data.details);
  assert(data.hideDetails);
  return showingDetails ? data.hideDetails : data.details;
}

// Sets up the proper button layout for the current platform.
function getButtonsCssClass(): string {
  let primaryControlOnLeft = true;
  // clang-format off
  // <if expr="is_macosx or is_ios or is_linux or is_chromeos or is_android">
  // clang-format on
  primaryControlOnLeft = false;
  // </if>

  return primaryControlOnLeft ? 'suggested-left' : 'suggested-right';
}

function onDocumentLoad() {
  onTemplateDataReceived(window.loadTimeDataRaw as TemplateData);
}

function onTemplateDataReceived(newData: TemplateData) {
  lastData = newData as TemplateData;
  render(getHtml(lastData, showingDetails), getRequiredElement('content'));

  if (!isSubFrame && newData.iconClass === 'icon-offline') {
    document.documentElement.classList.add('offline');
    // Set loadTimeData.data because it is used by the dino code.
    loadTimeData.data = newData;
    new Runner('.interstitial-wrapper');
  }
}

function getHtml(data: TemplateData, showingDetails: boolean) {
  // clang-format off
  return html`
    <div id="main-frame-error" class="interstitial-wrapper ${getMainFrameErrorCssClass(showingDetails)}">
      <div id="main-content">
        <div class="icon ${getMainFrameErrorIconCssClass(data)}"></div>
        <div id="main-message">
          <h1>
            <span .innerHTML="${data.heading.msg}"></span>
          </h1>
          ${data.summary ? html`
            <p .innerHTML="${data.summary.msg}"></p>
          ` : ''}

          ${shouldShowSuggestionsSummaryList(data) ? html`
            <div id="suggestions-list">
              <p>${data.suggestionsSummaryListHeader}</p>
              <ul class="${getSuggestionsSummaryItemCssClass(data)}">
                ${data.suggestionsSummaryList!.map(item => html`
                  <li .innerHTML="${item.summary}"></li>
                `)}
              </ul>
            </div>
          ` : ''}

          <div class="error-code">${data.errorCode}</div>

          ${data.savePageLater ? html`
            <div id="save-page-for-later-button">
              <a class="link-button" @click="${savePageLaterClick}">
                ${data.savePageLater.savePageMsg}
              </a>
            </div>
            <div id="cancel-save-page-button" class="hidden"
                @click="${cancelSavePageClick}"
                .innerHTML="${data.savePageLater.cancelMsg}">
            </div>
          ` : ''}
        </div>
      </div>
      <div id="buttons" class="nav-wrapper ${getButtonsCssClass()}">
        <div id="control-buttons" ?hidden="${!shouldShowControlButtons(data)}">
          ${data.reloadButton ? html`
            <button id="reload-button"
                class="blue-button text-button"
                @click="${reloadButtonClick}"
                data-url="${data.reloadButton.reloadUrl}">
              ${data.reloadButton.msg}
            </button>
          ` : ''}
          ${data.downloadButton ? html`
            <button id="download-button"
                class="blue-button text-button"
                @click="${downloadButtonClick}"
                .disabledText="${data.downloadButton.disabledMsg}">
              ${data.downloadButton.msg}
            </button>
          ` : ''}
        </div>
        ${shouldShowDetailsButton(data) ? html`
          <button id="details-button" class="secondary-button text-button
              small-link ${getDetailsButtonCssClass(data)}"
              @click="${detailsButtonClick}">
            ${getDetailsButtonText(data, showingDetails)}
          </button>
        ` : ''}
      </div>
      ${data.suggestionsDetails ? html`
        <div id="details">
          ${data.suggestionsDetails.map(item => html`
            <div class="suggestions">
              <div class="suggestion-header" .innerHTML="${item.header}"></div>
              <div class="suggestion-body" .innerHTML="${item.body}"></div>
            </div>
          `)}
        </div>
      ` : ''}
    </div>
    ${data.summary ? html`
      <div id="sub-frame-error">
        <!-- Show details when hovering over the icon, in case the details are
             hidden because they're too large. -->
        <div class="icon ${getSubFrameErrorIconCssClass(data)}"></div>
        <div id="sub-frame-error-details" .innerHTML="${data.summary.msg}">
        </div>
      </div>
    ` : ''}
  `;
  // clang-format on
}

// Expose methods that are triggered either
//  - By `onclick=...` handlers in the HTML code, OR
//  - By `href="javascript:..."` in localized links.
//  - By inected JS code coming from C++
//
//  since those need to be available on the 'window' object.
Object.assign(window, {
  diagnoseErrors,
  portalSignin,
  toggleHelpBox,
  updateForDnsProbe,
});

document.addEventListener('DOMContentLoaded', onDocumentLoad);
