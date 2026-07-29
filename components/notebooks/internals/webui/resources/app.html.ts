// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NotebooksInternalsAppElement} from './app.js';

export function getHtml(this: NotebooksInternalsAppElement) {
  // clang-format off
  return html`
<h1>Notebooks Internals</h1>

<section>
  <h2>Eligibility</h2>
  <table id="eligibilityTable">
    <thead>
      <tr>
        <th>Check</th>
        <th>Value</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>Is user eligible?</td>
        <td>
          ${this.eligibility?.userEligible ?
            html`<span class="valid">Yes</span>` :
            html`<span class="invalid">No</span>`}
        </td>
      </tr>
      <tr>
        <td><kbd>notebooks</kbd> feature flag enabled?</td>
        <td>
          ${this.featureFlags?.notebooksFeatureEnabled ?
            html`<span class="valid">Yes</span>` :
            html`<span class="invalid">No</span>`}
        </td>
      </tr>
    </tbody>
  </table>
</section>

<section>
  <h2>Configuration</h2>
  <table id="configTable">
    <thead>
      <tr>
        <th>Property</th>
        <th>Value</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>Notebook home URL</td>
        <td>${this.featureFlags?.notebookHomeUrl || '(default)'}</td>
      </tr>
    </tbody>
  </table>
</section>
  `;
  // clang-format on
}
