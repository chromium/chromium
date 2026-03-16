// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Bookmark} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export class TestBookmarksElement extends CrLitElement {
  static get is() {
    return 'test-bookmarks';
  }

  override render() {
    return this.bookmarks.map(item => {
      return html`
        <viewer-bookmark .bookmark="${item}" depth="0">
        </viewer-bookmark>`;
    });
  }

  static override get properties() {
    return {
      bookmarks: {type: Array},
    };
  }

  accessor bookmarks: Bookmark[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'test-bookmarks': TestBookmarksElement;
  }
}

customElements.define(TestBookmarksElement.is, TestBookmarksElement);
