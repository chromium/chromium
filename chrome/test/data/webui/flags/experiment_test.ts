// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/experiment.js';

import type {ExperimentElement} from 'chrome://flags/experiment.js';
import {FlagsBrowserProxyImpl} from 'chrome://flags/flags_browser_proxy.js';
import type {Feature} from 'chrome://flags/flags_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestFlagsBrowserProxy} from './test_flags_browser_proxy.js';

suite('ExperimentTest', function() {
  let experiment: ExperimentElement;
  let browserProxy: TestFlagsBrowserProxy;

  const experimentWithOptions: Feature = {
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

  const experimentWithoutOptions: Feature = {
    'description': 'available feature no default',
    'internal_name': 'available-feature-no-default',
    'is_default': true,
    'name': 'available feature on default',
    'enabled': true,
    'supported_platforms': ['Windows'],
  };

  function hasBeforePseudoElement(element: HTMLElement): boolean {
    const styles = window.getComputedStyle(element, '::before');
    return styles.content !== 'none';
  }

  function simulateUserInput(
      select: HTMLSelectElement, index: number): Promise<void> {
    select.selectedIndex = index;
    select.dispatchEvent(new Event('change'));
    return microtasksFinished();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFlagsBrowserProxy();
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    experiment = document.createElement('flags-experiment');
  });

  test('ExperimentWithOptions_Layout', async function() {
    experiment.data = experimentWithOptions;
    document.body.appendChild(experiment);
    await microtasksFinished();

    assertTrue(isVisible(experiment));
    const experimentName = experiment.getRequiredElement('.experiment-name');
    assertTrue(isVisible(experimentName));
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

    // Check that the UI has updates correctly when switching between default
    // and non-default states.
    assertEquals(0, select.selectedIndex);
    assertFalse(hasBeforePseudoElement(experimentName));
    await simulateUserInput(select, 1);
    assertTrue(hasBeforePseudoElement(experimentName));
    await simulateUserInput(select, 2);
    assertTrue(hasBeforePseudoElement(experimentName));
    await simulateUserInput(select, 0);
    assertFalse(hasBeforePseudoElement(experimentName));
  });

  test('ExperimentWithOptions_Change', async function() {
    experiment.data = experimentWithOptions;
    document.body.appendChild(experiment);
    await microtasksFinished();

    const select = experiment.getRequiredElement('select');
    assertEquals(0, select.selectedIndex);
    const whenFired = eventToPromise('select-change', select);
    await simulateUserInput(select, 1);

    return Promise.all([
      browserProxy.whenCalled('selectExperimentalFeature'),
      whenFired,
    ]);
  });

  test('ExperimentWithoutOptions_Layout', async function() {
    experiment.data = experimentWithoutOptions;
    document.body.appendChild(experiment);
    await microtasksFinished();

    assertTrue(isVisible(experiment));
    const experimentName = experiment.getRequiredElement('.experiment-name');
    assertTrue(isVisible(experimentName));
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
    assertEquals(1, select.selectedIndex);

    // Check that the UI has updates correctly when switching between default
    // and non-default states.
    assertFalse(hasBeforePseudoElement(experimentName));
    await simulateUserInput(select, 0);
    assertTrue(hasBeforePseudoElement(experimentName));
    await simulateUserInput(select, 1);
    assertFalse(hasBeforePseudoElement(experimentName));
  });

  test('ExperimentWithoutOptions_Change', async function() {
    experiment.data = experimentWithoutOptions;
    document.body.appendChild(experiment);
    await microtasksFinished();

    const select = experiment.getRequiredElement('select');
    assertEquals(1, select.selectedIndex);
    const whenFired = eventToPromise('select-change', select);
    await simulateUserInput(select, 0);

    return Promise.all([
      browserProxy.whenCalled('enableExperimentalFeature'),
      whenFired,
    ]);
  });

  test('UnavailableExperiment', async function() {
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
    await microtasksFinished();

    assertTrue(isVisible(experiment));
    const experimentName = experiment.getRequiredElement('.experiment-name');
    assertTrue(isVisible(experimentName));
    assertTrue(isVisible(experiment.getRequiredElement('.description')));
    assertTrue(isVisible(experiment.getRequiredElement('.platforms')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.links-container'));
    assertTrue(isVisible(experiment.getRequiredElement('.permalink')));
    assertFalse(!!experiment.shadowRoot!.querySelector('.textarea-container'));
    assertFalse(!!experiment.shadowRoot!.querySelector('.input-container'));

    const actions = experiment.getRequiredElement('.experiment-actions');
    assertTrue(!!actions);
    assertTrue(isVisible(actions));

    assertFalse(hasBeforePseudoElement(experimentName));
  });

  test('ExperimentWithLinks', async function() {
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
    await microtasksFinished();

    assertTrue(isVisible(experiment));
    assertTrue(isVisible(experiment.getRequiredElement('.links-container')));

    const links = experiment.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
        '.links-container a');
    assertEquals(1, links.length);
    const linkElement = links[0]!;
    assertEquals('https://a.com/', linkElement.href);
  });

  test('ExperimentWithTextInput', async function() {
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
    await microtasksFinished();

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

  test('ExperimentWithTextarea', async function() {
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
    await microtasksFinished();

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

  test('ExpandCollapseClick', async function() {
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
    await microtasksFinished();

    // Checking the 'expanded_' attribute since simulating the '@media
    // (max-width: 480px)' case in tests and then checking computed styles is
    // not possible (no control of the window's width).
    assertFalse(experiment.hasAttribute('expanded_'));
    const experimentName = experiment.getRequiredElement('.experiment-name');

    experimentName.click();
    await microtasksFinished();
    assertTrue(experiment.hasAttribute('expanded_'));


    experimentName.click();
    await microtasksFinished();
    assertFalse(experiment.hasAttribute('expanded_'));
  });
});
