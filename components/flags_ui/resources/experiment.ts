// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './experiment.html.js';
import {Feature, FlagsBrowserProxyImpl} from './flags_browser_proxy.js';

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


export class FlagsExperimentElement extends CustomElement {
  private feature_: Feature|null = null;

  static override get template() {
    return getTemplate();
  }

  set data(feature: Feature) {
    this.feature_ = feature;

    const container = this.getRequiredElement('.experiment');
    container.id = feature.internal_name;

    const experimentDefault = this.getRequiredElement('.experiment-default');
    experimentDefault.classList.toggle(
        'experiment-default', feature.is_default);
    experimentDefault.classList.toggle(
        'experiment-switched', !feature.is_default);

    const experimentName = this.getRequiredElement('.experiment-name');
    experimentName.id = `${feature.internal_name}_name`;
    experimentName.title =
        feature.is_default ? '' : loadTimeData.getString('experiment-enabled');
    experimentName.textContent = feature.name;

    const description = this.getRequiredElement('.description');
    description.textContent = feature.description;
    const platforms = this.getRequiredElement('.platforms');
    platforms.textContent = feature.supported_platforms.join(', ');

    if (feature.origin_list_value !== undefined) {
      const textarea = document.createElement('textarea');
      textarea.dataset['internalName'] = feature.internal_name;
      textarea.classList.add('experiment-origin-list-value');
      textarea.value = feature.origin_list_value;
      textarea.setAttribute('aria-labelledby', `${feature.internal_name}_name`);
      textarea.onchange = (e) => {
        e.stopPropagation();
        this.handleSetOriginListFlag_(textarea.value);
        textarea.dispatchEvent(new Event('textarea-change', {
          bubbles: true,
          composed: true,
        }));
      };
      this.getRequiredElement('.textarea-container').appendChild(textarea);
    }

    if (feature.string_value !== undefined) {
      const textbox = document.createElement('input');
      textbox.dataset['internalName'] = feature.internal_name;
      textbox.value = feature.string_value;
      textbox.setAttribute('aria-labelledby', `${feature.internal_name}_name`);
      textbox.onchange = (e) => {
        e.stopPropagation();
        this.handleSetStringFlag_(textbox.value);
        textbox.dispatchEvent(new Event('input-change', {
          bubbles: true,
          composed: true,
        }));
      };
      this.getRequiredElement('.input-container').appendChild(textbox);
    }

    const permalink = this.getRequiredElement<HTMLAnchorElement>('.permalink');
    permalink.href = `#${feature.internal_name}`;
    permalink.textContent = `#${feature.internal_name}`;

    const smallScreenCheck = window.matchMedia('(max-width: 480px)');
    // Toggling of experiment description overflow content on smaller screens.
    const expandContainer = this.getRequiredElement('.flex:first-child');
    if (smallScreenCheck.matches) {
      expandContainer.onclick = () =>
          expandContainer.classList.toggle('expand');
    }

    if (this.hasAttribute('unsupported')) {
      this.getRequiredElement('.experiment-actions').textContent =
          loadTimeData.getString('not-available-platform');
      return;
    }

    if (feature.options && feature.options.length > 0) {
      const experimentSelect = document.createElement('select');
      experimentSelect.dataset['internalName'] = feature.internal_name;
      experimentSelect.classList.add('experiment-select');
      experimentSelect.disabled = feature.enabled === false;
      experimentSelect.setAttribute(
          'aria-labelledby', `${feature.internal_name}_name`);

      for (let i = 0; i < feature.options.length; i++) {
        const option = feature.options[i]!;
        const optionEl = document.createElement('option');
        optionEl.selected = option.selected;
        optionEl.textContent = option.description;
        experimentSelect.appendChild(optionEl);
      }

      experimentSelect.onchange = (e) => {
        e.stopPropagation();
        this.handleSelectExperimentalFeatureChoice_(
            experimentSelect.selectedIndex);
        experimentSelect.dispatchEvent(new Event('select-change', {
          bubbles: true,
          composed: true,
        }));
      };

      this.getRequiredElement('.experiment-actions')
          .appendChild(experimentSelect);
      return;
    }

    assert(feature.options === undefined || feature.options.length === 0);
    const experimentEnableDisable = document.createElement('select');
    experimentEnableDisable.dataset['internalName'] = feature.internal_name;
    experimentEnableDisable.classList.add('experiment-enable-disable');
    experimentEnableDisable.setAttribute(
        'aria-labelledby', `${feature.internal_name}_name`);

    const disabledOptionEl = document.createElement('option');
    disabledOptionEl.value = 'disabled';
    disabledOptionEl.selected = !feature.enabled;
    disabledOptionEl.textContent = loadTimeData.getString('disabled');
    experimentEnableDisable.appendChild(disabledOptionEl);

    const enabledOptionEl = document.createElement('option');
    enabledOptionEl.value = 'enabled';
    enabledOptionEl.selected = feature.enabled;
    enabledOptionEl.textContent = loadTimeData.getString('enabled');
    experimentEnableDisable.appendChild(enabledOptionEl);

    experimentEnableDisable.onchange = (e) => {
      e.stopPropagation();
      const selectedIndex = experimentEnableDisable.selectedIndex;
      const selectedOption = experimentEnableDisable.options[selectedIndex]!;
      this.handleEnableExperimentalFeature_(selectedOption.value === 'enabled');
      experimentEnableDisable.dispatchEvent(new Event('select-change', {
        bubbles: true,
        composed: true,
      }));
    };

    this.getRequiredElement('.experiment-actions')
        .appendChild(experimentEnableDisable);
  }

  getSelect(): HTMLSelectElement|null {
    return this.$<HTMLSelectElement>('select');
  }

  getTextarea(): HTMLTextAreaElement|null {
    return this.$<HTMLTextAreaElement>('textarea');
  }

  getTextbox(): HTMLInputElement|null {
    return this.$<HTMLInputElement>('input');
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
   * Handles a 'enable' or 'disable' button getting clicked.
   * @param enable Whether to enable or disable the experiment.
   */
  private handleEnableExperimentalFeature_(enable: boolean) {
    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);
    assert(!this.feature_.options || this.feature_.options.length === 0);

    FlagsBrowserProxyImpl.getInstance().enableExperimentalFeature(
        this.feature_.internal_name, enable);

    const isDefault = enable ?
        (this.feature_.is_default === this.feature_.enabled) :
        (this.feature_.is_default !== this.feature_.enabled);

    this.updateDefaultStyle_(isDefault);
  }

  /**
   * Invoked when the selection of a multi-value choice is changed to the
   * specified index.
   * @param index The index of the option that was selected.
   */
  private handleSelectExperimentalFeatureChoice_(index: number) {
    /* This function is an onchange handler, which can be invoked during page
     * restore - see https://crbug.com/1038638. */
    assert(this.feature_);
    assert(this.feature_.options && this.feature_.options.length > 0);

    FlagsBrowserProxyImpl.getInstance().selectExperimentalFeature(
        this.feature_.internal_name, index);
    this.updateDefaultStyle_(index === 0);
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
    'flags-experiment': FlagsExperimentElement;
  }
}

customElements.define('flags-experiment', FlagsExperimentElement);
