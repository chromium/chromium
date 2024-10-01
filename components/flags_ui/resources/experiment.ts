// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './experiment.css.js';
import {getHtml} from './experiment.html.js';
import type {Feature} from './flags_browser_proxy.js';
import {FlagsBrowserProxyImpl} from './flags_browser_proxy.js';

/**
 * Parses the element's text content and highlights it with hit markers.
 * @param searchTerm The query to highlight for.
 * @param element The element to highlight.
 */
function highlightMatch(searchTerm: string, element: HTMLElement) {
  const text = element.textContent!;
  const match = text.toLowerCase().indexOf(searchTerm);

  // Assert against cases that are already handled before this function.
  assert(match !== -1);
  assert(searchTerm !== '');

  // Clear content.
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
      unsupported: {type: Boolean},

      isDefault_: {
        type: Boolean,
        reflect: true,
      },

      expanded_: {
        type: Boolean,
        reflect: true,
      },

      showingSearchHit_: {type: Boolean},
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

  // Whether the controls to change the experiment state should be hidden.
  unsupported: boolean = false;

  // Whether the currently selected value is the default value.
  protected isDefault_: boolean = false;

  // Whether the description text is expanded. Only has an effect on narrow
  // widths (max-width: 480px).
  protected expanded_: boolean = false;

  // Whether search hits are currently displayed. When true, some DOM nodes are
  // replaced with cloned nodes whose textContent is not rendered by Lit, so
  // that the highlight algorithm can freely modify them. Lit does not play
  // nicely with manual DOM modifications and throws internal errors in
  // subsequent renders.
  protected showingSearchHit_: boolean = false;

  getRequiredElement<K extends keyof HTMLElementTagNameMap>(query: K):
      HTMLElementTagNameMap[K];
  getRequiredElement<E extends HTMLElement = HTMLElement>(query: string): E;
  getRequiredElement(query: string) {
    const el = this.shadowRoot!.querySelector(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('feature_') ||
        changedProperties.has('unsupported')) {
      this.isDefault_ = this.computeIsDefault_();
    }
  }

  set data(feature: Feature) {
    this.feature_ = feature;
  }

  protected getExperimentTitle_(): string {
    if (this.showEnableDisableSelect_()) {
      return this.isDefault_ ? '' :
                               loadTimeData.getString('experiment-enabled');
    }

    return '';
  }

  protected getPlatforms_(): string {
    return this.feature_.supported_platforms.join(', ');
  }

  protected getHeaderId_(): string {
    return `${this.feature_.internal_name}_name`;
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

  protected onExperimentNameClick_(_e: Event) {
    this.expanded_ = !this.expanded_;
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
   * Looks for and highlights the first match on any of the component's title,
   * description, platforms and permalink text. Resets any pre-existing
   * highlights.
   * @param searchTerm The query to search for.
   * @return Whether or not a match was found.
   */
  async match(searchTerm: string): Promise<boolean> {
    this.showingSearchHit_ = false;

    if (searchTerm === '') {
      return true;
    }

    // Items to search in the desired order.
    const itemsToSearch = [
      this.feature_.name,
      this.feature_.description,
      this.getPlatforms_(),
      this.feature_.internal_name,
    ];

    const index = itemsToSearch.findIndex(item => {
      return item.toLowerCase().includes(searchTerm);
    });

    if (index === -1) {
      // No matches found. Nothing to do.
      return false;
    }

    // Render the clone elements (the ones to be highlighted with markers).
    this.showingSearchHit_ = true;
    await this.updateComplete;

    const queries = [
      '.clone.experiment-name',
      '.clone.body .description',
      '.clone.body .platforms',
      '.clone.permalink',
    ];

    // Populate clone elements with the proper text content.
    queries.forEach((query, i) => {
      const clone = this.getRequiredElement(query);
      clone.textContent = (i === 3 ? '#' : '') + itemsToSearch[i]!;
    });

    // Add highlights to the first clone element with matches.
    const cloneWithMatch = this.getRequiredElement(queries[index]!);
    highlightMatch(searchTerm, cloneWithMatch);
    return true;
  }

  protected computeIsDefault_(): boolean {
    if (this.showEnableDisableSelect_()) {
      const select = this.getRequiredElement('select');
      const enabled = select.value === 'enabled';
      return enabled ? (this.feature_.is_default === this.feature_.enabled) :
                       (this.feature_.is_default !== this.feature_.enabled);
    }

    if (this.showMultiValueSelect_()) {
      const select = this.getRequiredElement('select');
      return select.selectedIndex === 0;
    }

    return true;
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

    this.isDefault_ = this.computeIsDefault_();

    const experimentEnableDisable = e.target as HTMLSelectElement;
    FlagsBrowserProxyImpl.getInstance().enableExperimentalFeature(
        this.feature_.internal_name,
        experimentEnableDisable.value === 'enabled');
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

    this.isDefault_ = this.computeIsDefault_();

    const experimentSelect = e.target as HTMLSelectElement;
    FlagsBrowserProxyImpl.getInstance().selectExperimentalFeature(
        this.feature_.internal_name, experimentSelect.selectedIndex);
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
