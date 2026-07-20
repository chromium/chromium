// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import 'chrome://resources/cr_components/composebox/composebox_file_inputs.js';
import 'chrome://resources/cr_components/composebox/composebox_input.js';
import 'chrome://resources/cr_components/composebox/file_carousel.js';

import type {ComposeboxDropdownElement} from 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

const TestElementBase = ComposeboxEmbedderMixin(I18nMixinLit(CrLitElement));

export interface TestComposeboxMixinElement {
  $: {
    input: ComposeboxInputElement,
    matches: ComposeboxDropdownElement,
    inputWrapper: HTMLElement,
  };
}

export class TestComposeboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-composebox-mixin';
  }

  override render() {
    // clang-format off
    return html`
      <div id="inputWrapper" @keydown="${this.onKeydown}">
        <cr-composebox-input id="input"
            .result="${this.result}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}"
            @clear-smart-compose="${this.onClearSmartCompose}">
        </cr-composebox-input>
        <cr-composebox-dropdown id="matches"
            .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}">
        </cr-composebox-dropdown>
        <cr-composebox-file-inputs id="fileInputs"
            @file-change="${this.onFileChange}"
            .disableFileInputs="${this.shouldDisableFileInputs()}">
        </cr-composebox-file-inputs>
        ${this.showFileCarousel ? html`
          <cr-composebox-file-carousel
              id="carousel"
              .files="${this.getFilteredCarouselFiles()}"
              @delete-file="${this.onDeleteFile}">
          </cr-composebox-file-carousel>
        ` : ''}
      </div>
    `;
    // clang-format on
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  private activeElement_: Element|null = null;
  setActiveElement(elem: Element|null) {
    this.activeElement_ = elem;
  }

  override getActiveElement(): Element|null {
    return this.activeElement_ ?? this.shadowRoot.activeElement;
  }

  override getPageHandler() {
    return ComposeboxProxyImpl.getInstance().handler;
  }

  override getSearchboxCallbackRouter() {
    return ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
  }

  override getSearchboxHandler() {
    return ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getContextEntrypointElement(): ContextualEntrypointAndMenuElement
      |null {
    return null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-composebox-mixin': TestComposeboxMixinElement;
  }
}

customElements.define(
    TestComposeboxMixinElement.is, TestComposeboxMixinElement);
