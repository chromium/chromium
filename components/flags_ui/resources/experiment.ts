// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './experiment.html.js';
import {Feature} from './flags.js';

export class FlagsExperimentElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  set data(feature: Feature) {
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
      this.getRequiredElement('.textarea-container').appendChild(textarea);
    }

    const permalink = this.getRequiredElement<HTMLAnchorElement>('.permalink');
    permalink.href = `#${feature.internal_name}`;
    permalink.textContent = `#${feature.internal_name}`;

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
    disabledOptionEl.dataset['default'] = feature.is_default ?
        (!feature.enabled ? '1' : '0') :
        !feature.enabled ? '0' :
                           '1';
    experimentEnableDisable.appendChild(disabledOptionEl);

    const enabledOptionEl = document.createElement('option');
    enabledOptionEl.value = 'enabled';
    enabledOptionEl.selected = feature.enabled;
    enabledOptionEl.textContent = loadTimeData.getString('enabled');
    enabledOptionEl.dataset['default'] = feature.is_default ?
        (feature.enabled ? '1' : '0') :
        feature.enabled ? '0' :
                          '1';
    experimentEnableDisable.appendChild(enabledOptionEl);

    this.getRequiredElement('.experiment-actions')
        .appendChild(experimentEnableDisable);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'flags-experiment': FlagsExperimentElement;
  }
}

customElements.define('flags-experiment', FlagsExperimentElement);
