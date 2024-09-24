// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/experiment.js';

import type {ExperimentElement} from 'chrome://flags/experiment.js';
import {FlagsBrowserProxyImpl} from 'chrome://flags/flags_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestFlagsBrowserProxy} from './test_flags_browser_proxy.js';

suite('ExperimentTest', function() {
  let experiment: ExperimentElement;
  let browserProxy: TestFlagsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFlagsBrowserProxy();
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    experiment = document.createElement('flags-experiment');
  });

  test('check available experiments with options', function() {
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
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.links-container'));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.textarea-container'));
    assertFalse(!!experiment.shadowRoot!.querySelector('.input-container'));

    const select = experiment.getRequiredElement('select');
    assertTrue(isVisible(select));
    assertEquals(3, select.children.length);
    const options = ['Default', 'Enabled', 'Disabled'];
    for (let i = 0; i < select.children.length; ++i) {
      const option = select.children[i];
      assertTrue(option instanceof HTMLOptionElement);
      assertEquals(options[i], option.value);
    }
  });

  test('check available experiments without options', function() {
    experiment.data = {
      'description': 'available feature no default',
      'internal_name': 'available-feature-no-default',
      'is_default': true,
      'name': 'available feature on default',
      'enabled': true,
      'supported_platforms': ['Windows'],
    };
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.links-container'));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.textarea-container'));
    assertFalse(!!experiment.shadowRoot!.querySelector('.input-container'));

    const select = experiment.getRequiredElement('select');
    assertTrue(isVisible(select));
    assertEquals(2, select.options.length);
    assertEquals('disabled', select.options[0]!.value);
    assertEquals('enabled', select.options[1]!.value);
    assertEquals('enabled', select.value);
  });

  test('check unavailable experiments', function() {
    loadTimeData.data = {
      'not-available-platform': 'Not available on your platform.',
    };
    experiment.unsupported = true;
    experiment.data = {
      'description': 'unavailable feature',
      'internal_name': 'unavailable-feature',
      'is_default': true,
      'name': 'unavailable feature',
      'enabled': false,
      'supported_platforms': ['Windows'],
    };
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.experiment-name')));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.links-container'));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.textarea-container'));
    assertFalse(!!experiment.shadowRoot!.querySelector('.input-container'));

    const actions = experiment.getRequiredElement('.experiment-actions');
    assertTrue(!!actions);
    assertTrue(isVisible(actions));
  });

  test('check available experiments with links', function() {
    experiment.data = {
      'description': 'available feature with links',
      'internal_name': 'available-feature-with-links',
      'is_default': true,
      'name': 'available feature with links',
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
      'links': ['https://a.com'],
    };
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.links-container')));

    const links = experiment.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '.links-container a');
    assertEquals(1, links.length);
    const linkElement = links[0]!;
    assertEquals('https://a.com/', linkElement.href);
  });

  test('check experiment with text input', async function() {
    const data = {
      'description': 'Some Description',
      'internal_name': 'some-description',
      'is_default': true,
      'name': 'some description',
      'enabled': true,
      'supported_platforms': ['Windows'],
      'string_value': '',
    };
    experiment.data = data;
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertFalse(!!experiment.shadowRoot!.querySelector('.textarea-container'));

    const inputContainer =
        experiment.shadowRoot!.querySelector<HTMLInputElement>(
            '.input-container');
    assertTrue(!!inputContainer);
    assertTrue(isVisible(inputContainer));
    const input = inputContainer.querySelector('input');
    assertTrue(!!input);

    // Simulate user input.
    input.value = 'some value';
    const whenFired = eventToPromise('input-change', input);
    input.dispatchEvent(new CustomEvent('change'));

    await whenFired;
    const args = await browserProxy.whenCalled('setStringFlag');
    assertEquals(data.internal_name, args[0]);
    assertEquals(input.value, args[1]);
  });

  test('check experiment with textarea', async function() {
    const data = {
      'description': 'Some Description',
      'internal_name': 'some-description',
      'is_default': true,
      'name': 'some description',
      'enabled': true,
      'supported_platforms': ['Windows'],
      'origin_list_value': '',
    };
    experiment.data = data;
    document.body.appendChild(experiment);

    assertTrue(isVisible(experiment));
    assertFalse(!!experiment.shadowRoot!.querySelector('.input-container'));

    const textareaContainer =
        experiment.shadowRoot!.querySelector<HTMLInputElement>(
            '.textarea-container');
    assertTrue(!!textareaContainer);
    assertTrue(isVisible(textareaContainer));
    const textarea = textareaContainer.querySelector('textarea');
    assertTrue(!!textarea);

    // Simulate user input.
    textarea.value = 'some value';
    const whenFired = eventToPromise('textarea-change', textarea);
    textarea.dispatchEvent(new CustomEvent('change'));

    await whenFired;
    const args = await browserProxy.whenCalled('setOriginListFlag');
    assertEquals(data.internal_name, args[0]);
    assertEquals(textarea.value, args[1]);
  });

  test('ExpandCollapseClick', function() {
    const data = {
      'description': 'Some Description',
      'internal_name': 'some-description',
      'is_default': true,
      'name': 'some description',
      'enabled': true,
      'supported_platforms': ['Windows'],
    };
    experiment.data = data;
    document.body.appendChild(experiment);

    const experimentName = experiment.getRequiredElement('.experiment-name');
    const parentElement = experimentName.parentElement;
    assertTrue(!!parentElement);
    assertFalse(parentElement.classList.contains('expand'));

    experimentName.click();
    assertTrue(parentElement.classList.contains('expand'));

    experimentName.click();
    assertFalse(parentElement.classList.contains('expand'));
  });
});
