// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/experiment.js';

import type {FlagsExperimentElement} from 'chrome://flags/experiment.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ExperimentTest', function() {
  let experiment: FlagsExperimentElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    experiment = document.createElement('flags-experiment');
    document.body.appendChild(experiment);
  });

  test('check available experiments with default option', function() {
    experiment.data = {
      'description': 'available feature',
      'internal_name': 'available-feature',
      'is_default': true,
      'name': 'available feature',
      'enabled': true,
      'options': [
        {
          'description': 'Default',
          'internal_name': 'available-feature@0',
          'selected': false,
        },
        {
          'description': 'Enabled',
          'internal_name': 'available-feature@1',
          'selected': false,
        },
        {
          'description': 'Disabled',
          'internal_name': 'available-feature@2',
          'selected': false,
        },
      ],
      'supported_platforms': ['Windows'],
    };

    assertTrue(!!experiment);
    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertTrue(!!experiment.getRequiredElement('.textarea-container'));
    assertTrue(!!experiment.getRequiredElement('.input-container'));

    const select = experiment.getRequiredElement('select');
    assertTrue(!!select);
    assertTrue(isVisible(select));
    assertEquals(3, select.children.length);
    const options = ['Default', 'Enabled', 'Disabled'];
    for (let i = 0; i < select.children.length; ++i) {
      const option = select.children[i];
      assertTrue(option instanceof HTMLOptionElement);
      assertEquals(options[i], option.value);
    }
  });

  test('check available experiments without default option', function() {
    experiment.data = {
      'description': 'available feature no default',
      'internal_name': 'available-feature-no-default',
      'is_default': true,
      'name': 'available feature on default',
      'enabled': true,
      'options': [
        {
          'description': 'Enabled',
          'internal_name': 'available-feature@0',
          'selected': false,
        },
        {
          'description': 'Disabled',
          'internal_name': 'available-feature@1',
          'selected': false,
        },
      ],
      'supported_platforms': ['Windows'],
    };

    assertTrue(!!experiment);
    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertTrue(!!experiment.getRequiredElement('.textarea-container'));
    assertTrue(!!experiment.getRequiredElement('.input-container'));

    const select = experiment.getRequiredElement('select');
    assertTrue(!!select);
    assertTrue(isVisible(select));
    assertEquals(2, select.children.length);
    const options = ['Enabled', 'Disabled'];
    for (let i = 0; i < select.children.length; ++i) {
      const option = select.children[i];
      assertTrue(option instanceof HTMLOptionElement);
      assertEquals(options[i], option.value);
    }
  });

  test('check unavailable experiments', function() {
    loadTimeData.data = {
      'not-available-platform': 'Not available on your platform.',
    };
    experiment.toggleAttribute('unsupported', true);
    experiment.data = {
      'description': 'unavailable feature',
      'internal_name': 'unavailable-feature',
      'is_default': true,
      'name': 'unavailable feature',
      'enabled': false,
      'supported_platforms': ['Windows'],
    };

    assertTrue(!!experiment);
    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertTrue(!!experiment.getRequiredElement('.textarea-container'));
    assertTrue(!!experiment.getRequiredElement('.input-container'));

    const actions = experiment.getRequiredElement('.experiment-actions');
    assertTrue(!!actions);
    assertTrue(isVisible(actions));
  });
});
