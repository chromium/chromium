// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {TileSource} from 'chrome://resources/mojo/components/ntp_tiles/tile_source.mojom-webui.js';

interface PopularInfo {
  url: string;
  directory: string;
  country: string;
  version: string;
  json: string;

  overrideURL: string;
  overrideDirectory: string;
  overrideCountry: string;
  overrideVersion: string;
}

interface SourceInfo {
  popular: boolean|PopularInfo;
  topSites: boolean;
  customLinks: boolean;
  enterpriseShortcuts: boolean;
}

interface Icon {
  height: number;
  onDemand: boolean;
  type: string;
  url: string;
  width: number;
}

interface Site {
  icons: Icon[];
  lastVisitTime: number;
  score: number;
  source: TileSource;
  title: string;
  url: string;
  visitCount: number;
  fromMostVisited?: boolean;
}

function getSourcesHtml(
    state: SourceInfo, onSubmitUpdateClickFn: EventListener,
    onPopularViewJsonClickFn: EventListener) {
  const popularInfo = state.popular as PopularInfo;

  // clang-format off
  return html`
    <h2>Sources</h2>
    <table class="section-details">
      <tbody>
        <tr>
          <th colspan="2">TOP_SITES</th>
        </tr>
        <tr>
          <td class="detail">enabled</td>
          <td class="value">${state.topSites ? 'yes' : 'no'}</td>
        </tr>
      </tbody>
      <tbody>
        <tr>
          <th colspan="2">POPULAR</th>
        </tr>
        ${state.popular ? html`
          <tr>
            <td class="detail">URL</td>
            <td class="value">
              <input id="override-url" type="text"
                  .value="${popularInfo.overrideURL}"
                  placeholder="${popularInfo.url}">
            </td>
          </tr>
          <tr>
            <td class="detail">Country</td>
            <td class="value">
              <input id="override-directory" type="text"
                  .value="${popularInfo.overrideDirectory}"
                  placeholder="${popularInfo.directory}">
            </td>
          </tr>
          <tr>
            <td class="detail">Country</td>
            <td class="value">
              <input id="override-country" type="text"
                  .value="${popularInfo.overrideCountry}"
                  placeholder="${popularInfo.country}">
            </td>
          </tr>
          <tr>
            <td class="detail">Version</td>
            <td class="value">
              <input id="override-version" type="text"
                  .value="${popularInfo.overrideVersion}"
                  placeholder="${popularInfo.version}">
            </td>
          </tr>
          <tr>
            <td colspan="2">
              <button id="submit-update" @click="${onSubmitUpdateClickFn}">
                Update
              </button>
            </td>
          </tr>
          <tr>
            <td class="detail">
              <button id="popular-view-json" @click="${onPopularViewJsonClickFn}">
                View JSON
              </button>
            </td>
            <td class="value">
              <pre id="popular-json-value">${popularInfo.json}</pre>
            </td>
          </tr>
        ` : html`
          <tr>
            <td class="detail">enabled</td>
            <td class="value">no</td>
          </tr>
        `}
      </tbody>
      <tbody>
        <tr>
          <th colspan="2">CUSTOM_LINKS</th>
        </tr>
        <tr>
          <td class="detail">enabled</td>
          <td class="value">${state.customLinks ? 'yes' : 'no'}</td>
        </tr>
      </tbody>
        <tr>
          <th colspan="2">ENTEPRISE_SHORTCUTS</th>
        </tr>
        <tr>
          <td class="detail">enabled</td>
          <td class="value">${state.enterpriseShortcuts ? 'yes' : 'no'}</td>
        </tr>
      </tbody>
    </table>
  `;
  // clang-format on
}

function getSitesHtml(sites: Site[]) {
  // clang-format off
  return html`
    <h2>Sites</h2>
    <table class="section-details">
      ${sites.map(item => html`
        <tbody>
          <tr>
            <th colspan="2">${item.title}</th>
          </tr>
          <tr>
            <td class="detail">Source</td>
            <td class="value">${TileSource[item.source]}</td>
          </tr>
          <tr>
            <td class="detail">URL</td>
            <td class="value"><a href="${item.url}">${item.url}</a></td>
          </tr>
          ${item.icons.map(icon => html`
            <tr>
              <td class="detail">${icon.type}</td>
              <td class="value"><a href="${icon.url}">${icon.url}</a>
                (<span>${icon.width}x${icon.height}</span>
                ${icon.onDemand ? html`
                  <span class="value">, on-demand</span>
                ` : ''})
              </td>
            </tr>
          `)}
          ${item.visitCount > 0 ? html`
            <tr>
              <td class="detail">Visit Count</td>
              <td class="value">${item.visitCount}</td>
            </tr>
            <tr>
              <td class="detail">Last Visit Time</td>
              <td class="value">${item.lastVisitTime}</td>
            </tr>
          ` : ''}
          ${item.fromMostVisited !== undefined ? html`
            <tr>
              <td class="detail">From Most Visited</td>
              <td class="value">
                ${item.fromMostVisited ? 'yes' : 'no'}
              </td>
            </tr>
          ` : ''}
          ${item.score > 0 ? html`
            <tr>
              <td class="detail">Score</td>
              <td class="value">${item.score}</td>
            </tr>
          ` : ''}
        </tbody>
      `)}
    </table>`;
  // clang-format on
}

function onSubmitUpdateClick(event: Event) {
  event.preventDefault();
  chrome.send('update', [
    {
      popular: {
        overrideURL: getRequiredElement<HTMLInputElement>('override-url').value,
        overrideDirectory:
            getRequiredElement<HTMLInputElement>('override-directory').value,
        overrideCountry:
            getRequiredElement<HTMLInputElement>('override-country').value,
        overrideVersion:
            getRequiredElement<HTMLInputElement>('override-version').value,
      },
    },
  ]);
}

function onPopularViewJsonClick(event: Event) {
  event.preventDefault();
  if (getRequiredElement('popular-json-value').textContent === '') {
    chrome.send('viewPopularSitesJson');
  } else {
    getRequiredElement('popular-json-value').textContent = '';
  }
}

function initialize() {
  addWebUiListener('receive-source-info', (state: SourceInfo) => {
    render(
        getSourcesHtml(state, onSubmitUpdateClick, onPopularViewJsonClick),
        getRequiredElement('sources'));
  });
  addWebUiListener('receive-sites', (sites: {sites: Site[]}) => {
    render(getSitesHtml(sites.sites), getRequiredElement('sites'));
  });
  chrome.send('registerForEvents');
}

document.addEventListener('DOMContentLoaded', initialize);
