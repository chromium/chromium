// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// Custom sub-element to be used in the Lit version of the test help bubble
// host.
export class HelpBubbleMixinLitTestContainerElement extends CrLitElement {
  static get is() {
    return 'help-bubble-mixin-lit-test-container';
  }

  override render() {
    return html`
    <div>
      <div class='child-element'>ABCDE</div>
    </div>`;
  }
}

customElements.define(
    HelpBubbleMixinLitTestContainerElement.is,
    HelpBubbleMixinLitTestContainerElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble-mixin-lit-test-container':
        HelpBubbleMixinLitTestContainerElement;
  }
}
