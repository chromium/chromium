// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './experiment.css.js';
import {getHtml} from './experiment.html.js';
import type {Feature} from './flags_browser_proxy.js';
import {FlagsBrowserProxyImpl} from './flags_browser_proxy.js';

/**
 * Parses the element's text content to find the matching search term.
 * If a match is found, handles highlighting the substring.
 * @param searchTerm The query to search for.
 * @param element The element to search on and highlight.
 * @return Whether or not a match was found.
 */
function highlightMatch(searchTerm: string, element: HTMLElement): boolean {
  const text = element.textContent!;
  const match = text.toLowerCase().indexOf(searchTerm);

  if (match === -1) {
    return false;
  }

  if (searchTerm !== '') {
    // Clear all nodes.
    element.textContent = '';

    if (match > 0) {
      const textNodePrefix = document.createTextNode(text.substring(0, match));
      element.appendChild(textNodePrefix);
    }

    const matchEl = document.createElement('mark');
    matchEl.textContent = text.substr(match, searchTerm.length);
    element.appendChild(matchEl);

    const matchSuffix = text.substring(match + searchTerm.length);
    if (matchSuffix) {
      const textNodeSuffix = document.createTextNode(matchSuffix);
      element.appendChild(textNodeSuffix);
    }
  }
  return true;
}

/**
 * Reset existing highlights on an element.
 * @param element The element to remove all highlighted mark up on.
 */
function resetHighlights(element: HTMLElement) {
  if (element.children) {
    // Clear child <mark> from element, preserving inner text.
    element.textContent = element.textContent;
  }
}


export class ExperimentElement extends CrLitElement {
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      feature_: {type: Object},

      unsupported: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected feature_: Feature = {
    internal_name: '',
    name: '',
    description: '',
    enabled: false,
    is_default: false,
    supported_platforms: [],
  };

  unsupported: boolean = false;

  getRequiredElement<K extends keyof HTMLElementTagNameMap>(query: K):
      HTMLElementTagNameMap[K];
  getRequiredElement<E extends HTMLElement = HTMLElement>(query: string): E;
  getRequiredElement(query: string) {
    const el = this.shadowRoot!.querySelector(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }

  set data(feature: Feature) {
    this.feature_ = feature;
  }

  protected getExperimentCssClass_(): string {
    return this.feature_.is_default ? 'experiment-default' :
                                      'experiment-switched';
  }

  protected getExperimentTitle_(): string {
    return this.feature_.is_default ?
        '' :
        loadTimeData.getString('experiment-enabled');
  }

  protected getPlatforms_(): string {
    return this.feature_.supported_platforms.join(', ');
  }

  protected showEnableDisableSelect_(): boolean {
    return !this.unsupported &&
        (!this.feature_.options || !this.feature_.options.length);
  }

  protected showMultiValueSelect_(): boolean {
    return !this.unsupported && !!this.feature_.options &&
        !!this.feature_.options.length;
  }

  protected onTextareaChange_(e: Event) {
    e.stopPropagation();
    const textarea = e.target as HTMLTextAreaElement;
    this.handleSetOriginListFlag_(textarea.value);
    textarea.dispatchEvent(new Event('textarea-change', {
      bubbles: true,
      composed: true,
    }));
  }

  protected onTextInputChange_(e: Event) {
    e.stopPropagation();
    const textbox = e.target as HTMLInputElement;
    this.handleSetStringFlag_(textbox.value);
    textbox.dispatchEvent(new Event('input-change', {
      bubbles: true,
      composed: true,
    }));
  }

  protected onExperimentNameClick_(e: Event) {
    // Toggling of experiment description overflow content on smaller screens.
    // Only has an effect on narrow widths (max-width: 480px).
    (e.currentTarget as HTMLElement).parentElement!.classList.toggle('expand');
  }

  getSelect(): HTMLSelectElement|null {
    return this.shadowRoot!.querySelector('select');
  }

  getTextarea(): HTMLTextAreaElement|null {
    return this.shadowRoot!.querySelector('textarea');
  }

  getTextbox(): HTMLInputElement|null {
    return this.shadowRoot!.querySelector('input');
  }

  /**
   * Looks for and highlights the first match on any of the
   * component's title, body, and permalink text. Resets
   * preexisting highlights.
   * @param searchTerm The query to search for.
   * @return Whether or not a match was found.
   */
  match(searchTerm: string): boolean {
    const title = this.getRequiredElement('.experiment-name');
    const body = this.getRequiredElement('.body');
    const permalink = this.getRequiredElement('.permalink');

    resetHighlights(title);
    resetHighlights(body);
    resetHighlights(permalink);

    return highlightMatch(searchTerm, title) ||
        highlightMatch(searchTerm, body) ||
        highlightMatch(searchTerm.replace(/\s/, '-'), permalink);
  }


  /**
   * Sets style depending on the selected option.
   * @param isDefault Whether or not the default option is selected.
   */
  private updateDefaultStyle_(isDefault: boolean) {
    const experimentContainer =
        this.getRequiredElement('.experiment-default, .experiment-switched');
    experimentContainer.classList.toggle('experiment-default', isDefault);
    experimentContainer.classList.toggle('experiment-switched', !isDefault);
  }

  /**
   * Invoked when the selection of an enable/disable choice is changed.
   */
  protected onExperimentEnableDisableChange_(e: Event) {
    e.stopPropagation();

    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);
    assert(!this.feature_.options || this.feature_.options.length === 0);

    const experimentEnableDisable = e.target as HTMLSelectElement;
    const enable = experimentEnableDisable.value === 'enabled';
    FlagsBrowserProxyImpl.getInstance().enableExperimentalFeature(
        this.feature_.internal_name, enable);

    const isDefault = enable ?
        (this.feature_.is_default === this.feature_.enabled) :
        (this.feature_.is_default !== this.feature_.enabled);

    this.updateDefaultStyle_(isDefault);

    experimentEnableDisable.dispatchEvent(new Event('select-change', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * Invoked when the selection of a multi-value choice is changed.
   */
  protected onExperimentSelectChange_(e: Event) {
    e.stopPropagation();

    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);
    assert(this.feature_.options && this.feature_.options.length > 0);

    const experimentSelect = e.target as HTMLSelectElement;
    const index = experimentSelect.selectedIndex;

    FlagsBrowserProxyImpl.getInstance().selectExperimentalFeature(
        this.feature_.internal_name, index);
    this.updateDefaultStyle_(index === 0);

    experimentSelect.dispatchEvent(new Event('select-change', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * Invoked when the value of a textarea for origins is changed.
   * @param value The value of the textarea.
   */
  private handleSetOriginListFlag_(value: string) {
    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);
    assert(this.feature_.origin_list_value !== undefined);

    FlagsBrowserProxyImpl.getInstance().setOriginListFlag(
        this.feature_.internal_name, value);
  }

  /**
   * Invoked when the value of an input is changed.
   * @param value The value of the input.
   */
  private handleSetStringFlag_(value: string) {
    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);

    FlagsBrowserProxyImpl.getInstance().setStringFlag(
        this.feature_.internal_name, value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'flags-experiment': ExperimentElement;
  }
}

customElements.define('flags-experiment', ExperimentElement);
