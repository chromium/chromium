// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import 'chrome://resources/cr_components/composebox/composebox_file_inputs.js';
import 'chrome://resources/cr_components/composebox/composebox_input.js';
import 'chrome://resources/cr_components/composebox/composebox_submit.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import 'chrome://resources/cr_components/composebox/file_carousel.js';
import 'chrome://resources/cr_components/search/animated_glow.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {getCss} from 'chrome://resources/cr_components/composebox/composebox.css.js';
import type {ComposeboxDropdownElement} from 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

const TestElementBase = ComposeboxEmbedderMixin(I18nMixinLit(CrLitElement));

export interface TestComposeboxMixinElement {
  $: {
    animatedSearchElement: SearchAnimatedGlowElement,
    composebox: HTMLElement,
    contextEntrypoint: ContextualEntrypointAndMenuElement,
    input: ComposeboxInputElement,
    inputWrapper: HTMLElement,
    matches: ComposeboxDropdownElement,
    voiceSearch: ComposeboxVoiceSearchElement,
  };
}

export class TestComposeboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-composebox-mixin';
  }

  static override get styles() {
    return getCss();
  }


  override render() {
    // clang-format off
    return html`
      <search-animated-glow id="animatedSearchElement"
          animation-state="${this.animationState}"
          .coloredTicTacVoiceAnimationEnabled="${this.voiceSearchCoherenceEnabled}"
          .requiresVoice="${this.shouldShowVoiceSearchAnimation()}"
          .transcript="${this.transcript}"
          .receivedSpeech="${this.receivedSpeech}"
          .isListening="${this.isListening}"
          .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}"
          .darkThemeColorsEnabled="${false}">
      </search-animated-glow>
      <div id="composebox" @keydown="${this.onKeydown}"
          @dragenter="${this.dragAndDropHandler.handleDragEnter}"
          @dragover="${this.dragAndDropHandler.handleDragOver}"
          @dragleave="${this.dragAndDropHandler.handleDragLeave}"
          @drop="${this.dragAndDropHandler.handleDrop}">
        <div id="inputWrapper">
          <cr-composebox-input id="input"
              .result="${this.result}"
              .input="${this.input}"
              .inputPlaceholder="${this.inputPlaceholder}"
              .smartComposeEnabled="${this.smartComposeEnabled}"
              .smartComposeInlineHint="${this.smartComposeInlineHint}"
              .cancelButtonTitle="${this.computeCancelButtonTitle()}"
              @input-input="${this.onInputInput}"
              @input-focusin="${this.onInputFocusin}"
              @cancel-click="${this.onCancelClick}"
              @clear-smart-compose="${this.onClearSmartCompose}">
            ${this.shouldShowVoiceSearch() ? html`
              <cr-icon-button id="voiceSearchButton" class="voice-icon"
                  slot="action-buttons"
                  part="voice-icon" iron-icon="cr:mic-filled"
                  @click="${this.onVoiceSearchButtonClick}"
                  title="${this.i18n('voiceSearchButtonLabel')}">
              </cr-icon-button>
            ` : ''}
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
          <cr-composebox-contextual-entrypoint-and-menu
              id="contextEntrypoint"
              .inputState="${this.inputState}"
              @tool-click="${this.onToolClick}">
          </cr-composebox-contextual-entrypoint-and-menu>
          <cr-composebox-submit
              ?disabled="${!this.canSubmitFilesAndInput}"
              .iconType="${this.submitButtonIconType}"
              .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
              @submit-click="${this.onSubmitClick}"
              @submit-focusin="${this.onSubmitFocusin}">
          </cr-composebox-submit>
        </div>
      </div>
      ${this.shouldShowVoiceSearch() ? html`
        <cr-composebox-voice-search id="voiceSearch"
            @voice-permission-changed="${this.onVoicePermissionChanged}"
            @voice-search-cancel="${this.onVoiceSearchCancel}"
            @voice-search-final-result="${this.onVoiceSearchFinalResult}"
            @voice-search-error="${this.onVoiceSearchError}"
            @transcript-update="${this.onTranscriptUpdate}"
            @speech-received="${this.onSpeechReceived}"
            @recording-stopped="${this.onRecordingStopped}"
            .submitStopButtonsEnabled="${this.voiceSearchCoherenceEnabled}"
            .liveTranscriptEnabled="${!this.voiceSearchCoherenceEnabled}"
            .submitButtonIconType="${this.submitButtonIconType}"
            .dynamicTimeoutEnabled="${false}"
            .pageCallbackRouter="${this.getSearchboxCallbackRouter()}"
            exportparts="voice-close-button, voice-details-link, voice-stop-button, voice-submit-button">
        </cr-composebox-voice-search>
      ` : ''}
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
    return this.$.composebox;
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

  searchboxCallbackRouter: SearchboxPageCallbackRouter =
      ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
  override getSearchboxCallbackRouter() {
    return this.searchboxCallbackRouter;
  }

  override getSearchboxHandler() {
    return ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getContextEntrypointElement(): ContextualEntrypointAndMenuElement {
    return this.$.contextEntrypoint;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-composebox-mixin': TestComposeboxMixinElement;
  }
}

customElements.define(
    TestComposeboxMixinElement.is, TestComposeboxMixinElement);
