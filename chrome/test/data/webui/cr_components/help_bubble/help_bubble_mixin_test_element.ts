// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_bubble_mixin_test_container.js';

import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_interface.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export const HelpBubbleMixinTestElementBase =
    HelpBubbleMixin(PolymerElement) as {
      new (): PolymerElement & HelpBubbleMixinInterface,
    };

export interface HelpBubbleMixinTestElement {
  $: {
    bulletList: HTMLElement,
    container: HTMLElement,
    p1: HTMLElement,
    title: HTMLElement,
  };
}

// Polymer test element demonstrating help bubble functionality.
export class HelpBubbleMixinTestElement extends HelpBubbleMixinTestElementBase {
  static get is() {
    return 'help-bubble-mixin-test';
  }

  static get template() {
    return html`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='p1'>Some paragraph text</p>
      <ul id='bulletList'>
        <li id='list-item'>List item 1</li>
        <li>List item 2</li>
      </ul>
      <span style='display: block;'>Span text</span>
      <help-bubble-mixin-test-container id='container-element'>
      </help-bubble-mixin-test-container>
      <div id="custom-container"></div>
      <div id="custom-anchor">Custom Anchor</div>
    </div>`;
  }
}

customElements.define(
    HelpBubbleMixinTestElement.is, HelpBubbleMixinTestElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble-mixin-test': HelpBubbleMixinTestElement;
  }
}
