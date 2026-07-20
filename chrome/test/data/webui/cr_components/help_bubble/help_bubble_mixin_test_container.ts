// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Custom sub-element to be used in the Polymer version of the test help bubble
// host.
export class HelpBubbleMixinTestContainerElement extends PolymerElement {
  static get is() {
    return 'help-bubble-mixin-test-container';
  }

  static get template() {
    return html`
    <div>
      <div class='child-element'>ABCDE</div>
    </div>`;
  }
}

customElements.define(
    HelpBubbleMixinTestContainerElement.is,
    HelpBubbleMixinTestContainerElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble-mixin-test-container': HelpBubbleMixinTestContainerElement;
  }
}
