// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_bubble_mixin_lit_test_container.js';

import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export const HelpBubbleMixinLitTestElementBase =
    HelpBubbleMixinLit(CrLitElement);

export interface HelpBubbleMixinLitTestElement {
  $: {
    bulletList: HTMLElement,
    container: HTMLElement,
    p1: HTMLElement,
    title: HTMLElement,
  };
}

// Lit test element demonstrating help bubble functionality.
export class HelpBubbleMixinLitTestElement extends
    HelpBubbleMixinLitTestElementBase {
  static get is() {
    return 'help-bubble-mixin-lit-test';
  }

  override render() {
    return html`
    <div id="container">
      <h1 id="title">This is the title</h1>
      <p id="p1">Some paragraph text</p>
      <ul id="bulletList">
        <li id="list-item">List item 1</li>
        <li>List item 2</li>
      </ul>
      <span style="display: block;">Span text</span>
      <help-bubble-mixin-lit-test-container id="container-element">
      </help-bubble-mixin-lit-test-container>
      <div id="custom-container"></div>
      <div id="custom-anchor">Custom Anchor</div>
    </div>`;
  }
}

customElements.define(
    HelpBubbleMixinLitTestElement.is, HelpBubbleMixinLitTestElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble-mixin-lit-test': HelpBubbleMixinLitTestElement;
  }
}
