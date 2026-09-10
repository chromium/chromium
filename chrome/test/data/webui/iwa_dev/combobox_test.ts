// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/combobox.js';

import type {ComboboxOption, IwaDevComboboxElement} from 'chrome://iwa-dev/combobox.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-combobox>', () => {
  let combobox: IwaDevComboboxElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    combobox = document.createElement('iwa-dev-combobox');
    document.body.appendChild(combobox);
    await microtasksFinished();
  });

  function getSuggestions(): HTMLElement|null {
    return combobox.shadowRoot.querySelector('#suggestions');
  }

  function isSuggestionsOpen(): boolean {
    const suggestions = getSuggestions();
    return !!suggestions && suggestions.matches(':popover-open');
  }

  function getSuggestionItems(): NodeListOf<HTMLButtonElement> {
    return combobox.shadowRoot.querySelectorAll<HTMLButtonElement>(
        '#suggestions .suggestion-item');
  }

  function getDropdownButton(): HTMLElement|null {
    return combobox.shadowRoot.querySelector('#dropdownButton');
  }

  function getClearButton(): HTMLElement|null {
    return combobox.shadowRoot.querySelector('#clearButton');
  }

  test('initializes with default values and renders input', () => {
    assertEquals('', combobox.value);
    assertEquals('', combobox.label);
    assertEquals('', combobox.placeholder);
    assertFalse(combobox.clearable);
    assertFalse(combobox.disabled);
    assertFalse(combobox.readonly);
    assertEquals('prefix', combobox.matchStrategy);
    assertEquals(0, combobox.options.length);
    assertEquals(null, getDropdownButton());
    assertEquals(null, getSuggestions());

    const input = combobox.inputElement;
    assertTrue(!!input);
    assertEquals('', input.value);
    assertEquals('combobox', input.getAttribute('role'));
    assertEquals('list', input.getAttribute('aria-autocomplete'));
    assertEquals('listbox', input.getAttribute('aria-haspopup'));
    assertEquals('false', input.getAttribute('aria-expanded'));
    assertFalse(input.hasAttribute('aria-controls'));
    assertFalse(input.hasAttribute('aria-activedescendant'));
  });

  test(
      'does not render dropdown or suggestions when options is empty',
      async () => {
        assertEquals(null, getDropdownButton());
        assertEquals(null, getSuggestions());

        // Focusing input should not open suggestions when options is empty.
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertEquals(null, getSuggestions());

        // ArrowDown should not open suggestions when options is empty.
        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        assertEquals(null, getSuggestions());

        // Populate options and then clear to verify transition back to empty.
        combobox.options = [{value: 'v1'}];
        await microtasksFinished();
        assertTrue(!!getDropdownButton());
        assertTrue(!!getSuggestions());

        combobox.options = [];
        await microtasksFinished();
        assertEquals(null, getDropdownButton());
        assertEquals(null, getSuggestions());
      });

  test('focus() delegates focus to internal input', () => {
    combobox.focus();
    assertEquals(combobox.inputElement, combobox.shadowRoot.activeElement);
  });

  test('reflects label and placeholder', async () => {
    combobox.label = 'Update Channel';
    combobox.placeholder = 'Select channel';
    await microtasksFinished();

    const label = combobox.shadowRoot.querySelector('label');
    assertTrue(!!label);
    assertEquals('Update Channel', label.textContent.trim());
    assertEquals('Select channel', combobox.inputElement.placeholder);

    combobox.options = [{value: 'v1'}];
    await microtasksFinished();
    assertEquals(
        'Update Channel', getSuggestions()!.getAttribute('aria-label'));
  });

  test('disabled state disables input and icon buttons', async () => {
    combobox.options = [{value: 'opt1'}];
    combobox.clearable = true;
    combobox.value = 'opt1';
    combobox.disabled = true;
    await microtasksFinished();

    assertTrue(combobox.inputElement.disabled);
    const clearButton = getClearButton();
    assertTrue(!!clearButton);
    assertTrue(clearButton.hasAttribute('disabled'));

    const dropdownButton = getDropdownButton();
    assertTrue(!!dropdownButton);
    assertTrue(dropdownButton.hasAttribute('disabled'));
  });

  test('clearable button visibility and behavior', async () => {
    combobox.clearable = false;
    combobox.value = 'hello';
    await microtasksFinished();
    assertEquals(null, getClearButton());

    combobox.clearable = true;
    combobox.value = '';
    await microtasksFinished();
    assertEquals(null, getClearButton());

    combobox.value = 'hello';
    combobox.errorMessage = 'Some error';
    await microtasksFinished();
    const clearButton = getClearButton();
    assertTrue(!!clearButton);

    const valueChangedPromise =
        eventToPromise<CustomEvent<{value: string}>>('value-changed', combobox);
    clearButton.click();
    const event = await valueChangedPromise;

    assertEquals('', event.detail.value);
    assertEquals('', combobox.value);
    assertEquals('', combobox.errorMessage);
    assertEquals(combobox.inputElement, combobox.shadowRoot.activeElement);
    assertEquals(null, getSuggestions());
  });

  test('dropdown button toggles suggestions', async () => {
    assertEquals(null, getDropdownButton());

    const options: ComboboxOption[] = [
      {value: 'val1', label: 'Option 1'},
      {value: 'val2', label: 'Option 2'},
    ];
    combobox.options = options;
    await microtasksFinished();

    const dropdownButton = getDropdownButton();
    assertTrue(!!dropdownButton);
    assertEquals('Show suggestions', dropdownButton.getAttribute('title'));

    dropdownButton.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertTrue(isSuggestionsOpen());
    assertEquals(combobox.inputElement, combobox.shadowRoot.activeElement);
    assertEquals('true', combobox.inputElement.getAttribute('aria-expanded'));
    assertEquals(
        'suggestions', combobox.inputElement.getAttribute('aria-controls'));
    assertEquals('Hide suggestions', dropdownButton.getAttribute('title'));

    dropdownButton.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertFalse(isSuggestionsOpen());
    assertEquals('false', combobox.inputElement.getAttribute('aria-expanded'));
    assertFalse(combobox.inputElement.hasAttribute('aria-controls'));
    assertEquals('Show suggestions', dropdownButton.getAttribute('title'));
  });

  test('displays options with label or fallback to value', async () => {
    combobox.options = [
      {value: 'v1', label: 'Version 1'},
      {value: 'v2'},
    ];
    await microtasksFinished();

    getDropdownButton()!.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    const items = getSuggestionItems();
    assertEquals(2, items.length);
    assertEquals('Version 1', items[0]!.textContent.trim());
    assertEquals('v2', items[1]!.textContent.trim());
  });

  test('aria-selected is set on matching option', async () => {
    combobox.options = [{value: 'v1'}, {value: 'v2'}];
    combobox.value = 'v2';
    await microtasksFinished();

    getDropdownButton()!.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    const items = getSuggestionItems();
    assertEquals('false', items[0]!.getAttribute('aria-selected'));
    assertEquals('true', items[1]!.getAttribute('aria-selected'));
  });

  test(
      'selecting an option via pointerdown updates value and fires event',
      async () => {
        combobox.options = [{value: 'alpha'}, {value: 'beta'}];
        await microtasksFinished();

        getDropdownButton()!.dispatchEvent(
            new PointerEvent('pointerdown', {bubbles: true}));
        await microtasksFinished();

        const items = getSuggestionItems();
        assertEquals(2, items.length);

        const valueChangedPromise =
            eventToPromise<CustomEvent<{value: string}>>(
                'value-changed', combobox);
        items[1]!.dispatchEvent(
            new PointerEvent('pointerdown', {bubbles: true}));
        const event = await valueChangedPromise;
        await microtasksFinished();

        assertEquals('beta', event.detail.value);
        assertEquals('beta', combobox.value);
        assertFalse(isSuggestionsOpen());
        assertEquals(combobox.inputElement, combobox.shadowRoot.activeElement);
      });

  test('filters options using prefix matchStrategy by default', async () => {
    combobox.options = [
      {value: 'apple'},
      {value: 'banana'},
      {value: 'pineapple'},
    ];
    await microtasksFinished();

    const valueChangedPromise =
        eventToPromise<CustomEvent<{value: string}>>('value-changed', combobox);
    combobox.inputElement.value = 'app';
    combobox.inputElement.dispatchEvent(new Event('input', {bubbles: true}));
    const event = await valueChangedPromise;
    await microtasksFinished();

    assertEquals('app', event.detail.value);
    assertEquals('app', combobox.value);
    const items = getSuggestionItems();
    assertEquals(1, items.length);
    assertEquals('apple', items[0]!.textContent.trim());
  });

  test('filters options trimming leading and trailing whitespace', async () => {
    combobox.options = [
      {value: 'apple'},
      {value: 'banana'},
    ];
    await microtasksFinished();

    combobox.inputElement.value = '  apple  ';
    combobox.inputElement.dispatchEvent(new Event('input', {bubbles: true}));
    await microtasksFinished();

    const items = getSuggestionItems();
    assertEquals(1, items.length);
    assertEquals('apple', items[0]!.textContent.trim());
  });

  test(
      'typing query with zero matches closes suggestions popover', async () => {
        combobox.options = [{value: 'apple'}, {value: 'banana'}];
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());

        combobox.inputElement.value = 'xyz';
        combobox.inputElement.dispatchEvent(
            new Event('input', {bubbles: true}));
        await microtasksFinished();

        assertFalse(isSuggestionsOpen());
        assertEquals(
            'false', combobox.inputElement.getAttribute('aria-expanded'));
        assertFalse(combobox.inputElement.hasAttribute('aria-controls'));
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        // Clearing query reopens suggestions.
        combobox.inputElement.value = '';
        combobox.inputElement.dispatchEvent(
            new Event('input', {bubbles: true}));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());
        assertEquals(2, getSuggestionItems().length);
      });

  test('typing input clears errorMessage and fires value-changed', async () => {
    combobox.options = [{value: 'alpha'}];
    combobox.errorMessage = 'Invalid entry';
    await microtasksFinished();

    const valueChangedPromise =
        eventToPromise<CustomEvent<{value: string}>>('value-changed', combobox);
    combobox.inputElement.value = 'alp';
    combobox.inputElement.dispatchEvent(new Event('input', {bubbles: true}));
    const event = await valueChangedPromise;
    await microtasksFinished();

    assertEquals('alp', event.detail.value);
    assertEquals('', combobox.errorMessage);
    assertTrue(isSuggestionsOpen());
  });

  test('filters options using contains matchStrategy', async () => {
    combobox.matchStrategy = 'contains';
    combobox.options = [
      {value: 'apple'},
      {value: 'banana'},
      {value: 'pineapple'},
    ];
    await microtasksFinished();

    combobox.inputElement.value = 'apple';
    combobox.inputElement.dispatchEvent(new Event('input', {bubbles: true}));
    await microtasksFinished();

    const items = getSuggestionItems();
    assertEquals(2, items.length);
    assertEquals('apple', items[0]!.textContent.trim());
    assertEquals('pineapple', items[1]!.textContent.trim());
  });

  test('filters matching option label as well as value', async () => {
    combobox.matchStrategy = 'contains';
    combobox.options = [
      {value: 'channel_1', label: 'Stable Channel'},
      {value: 'channel_2', label: 'Beta Channel'},
    ];
    await microtasksFinished();

    combobox.inputElement.value = 'beta';
    combobox.inputElement.dispatchEvent(new Event('input', {bubbles: true}));
    await microtasksFinished();

    const items = getSuggestionItems();
    assertEquals(1, items.length);
    assertEquals('Beta Channel', items[0]!.textContent.trim());
  });

  test(
      'readonly mode shows label, disables manual typing, and toggles on ' +
          'input click',
      async () => {
        combobox.readonly = true;
        combobox.options = [
          {value: 'ch-stable', label: 'Stable'},
          {value: 'ch-beta', label: 'Beta'},
        ];
        combobox.value = 'ch-stable';
        await microtasksFinished();

        assertTrue(combobox.inputElement.readOnly);
        assertEquals(
            'none', combobox.inputElement.getAttribute('aria-autocomplete'));
        assertEquals('Stable', combobox.inputElement.value);

        // Focusing input in readonly mode should not open suggestions.
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertFalse(isSuggestionsOpen());

        // First click toggles open and prevents default to suppress text
        // cursor.
        const firstClick =
            new PointerEvent('pointerdown', {bubbles: true, cancelable: true});
        combobox.inputElement.dispatchEvent(firstClick);
        assertTrue(firstClick.defaultPrevented);
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());

        // Second click toggles closed and prevents default.
        const secondClick =
            new PointerEvent('pointerdown', {bubbles: true, cancelable: true});
        combobox.inputElement.dispatchEvent(secondClick);
        assertTrue(secondClick.defaultPrevented);
        await microtasksFinished();
        assertFalse(isSuggestionsOpen());

        // Typing does not modify value.
        combobox.inputElement.value = 'typed text';
        combobox.inputElement.dispatchEvent(
            new Event('input', {bubbles: true}));
        await microtasksFinished();

        assertEquals('ch-stable', combobox.value);
      });

  test('readonly mode falls back to value when label is absent', async () => {
    combobox.readonly = true;
    combobox.options = [{value: 'raw-value'}];
    combobox.value = 'raw-value';
    await microtasksFinished();

    assertEquals('raw-value', combobox.inputElement.value);
  });

  test('editable mode input pointerdown does not prevent default', async () => {
    const pointerDownEvent =
        new PointerEvent('pointerdown', {bubbles: true, cancelable: true});
    combobox.inputElement.dispatchEvent(pointerDownEvent);
    await microtasksFinished();

    assertFalse(pointerDownEvent.defaultPrevented);
  });

  test(
      'keyboard navigation with ArrowDown and ArrowUp wraps around',
      async () => {
        combobox.options =
            [{value: 'first'}, {value: 'second'}, {value: 'third'}];
        await microtasksFinished();

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());

        let items = getSuggestionItems();
        assertTrue(items[0]!.classList.contains('highlighted'));
        assertEquals(
            'suggestion-0',
            combobox.inputElement.getAttribute('aria-activedescendant'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        items = getSuggestionItems();
        assertTrue(items[1]!.classList.contains('highlighted'));
        assertEquals(
            'suggestion-1',
            combobox.inputElement.getAttribute('aria-activedescendant'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        items = getSuggestionItems();
        assertTrue(items[2]!.classList.contains('highlighted'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        items = getSuggestionItems();
        assertTrue(items[0]!.classList.contains('highlighted'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
        await microtasksFinished();
        items = getSuggestionItems();
        assertTrue(items[2]!.classList.contains('highlighted'));
      });

  test(
      'keyboard navigation: ArrowDown on focused combobox with matching' +
          ' value highlights matching option',
      async () => {
        combobox.options =
            [{value: 'first'}, {value: 'second'}, {value: 'third'}];
        combobox.value = 'second';
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();

        assertEquals(
            'suggestion-1',
            combobox.inputElement.getAttribute('aria-activedescendant'));
        const items = getSuggestionItems();
        assertTrue(items[1]!.classList.contains('highlighted'));
      });

  test(
      'keyboard navigation: ArrowUp on empty focused combobox jumps to last' +
          ' option',
      async () => {
        combobox.options =
            [{value: 'first'}, {value: 'second'}, {value: 'third'}];
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
        await microtasksFinished();

        assertEquals(
            'suggestion-2',
            combobox.inputElement.getAttribute('aria-activedescendant'));
        const items = getSuggestionItems();
        assertTrue(items[2]!.classList.contains('highlighted'));
      });

  test('keyboard navigation: Enter selects highlighted option', async () => {
    combobox.options = [{value: 'first'}, {value: 'second'}];
    combobox.focus();
    combobox.inputElement.dispatchEvent(new Event('focus'));
    await microtasksFinished();

    combobox.inputElement.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
    await microtasksFinished();

    combobox.inputElement.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
    await microtasksFinished();

    const valueChangedPromise =
        eventToPromise<CustomEvent<{value: string}>>('value-changed', combobox);
    combobox.inputElement.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    const event = await valueChangedPromise;
    await microtasksFinished();

    assertEquals('second', event.detail.value);
    assertEquals('second', combobox.value);
    assertFalse(isSuggestionsOpen());
  });

  test(
      'keyboard navigation: Enter closes suggestions when no option is' +
          ' highlighted',
      async () => {
        combobox.options = [{value: 'first'}, {value: 'second'}];
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        const enterEvent = new KeyboardEvent(
            'keydown', {key: 'Enter', bubbles: true, cancelable: true});
        combobox.inputElement.dispatchEvent(enterEvent);
        await microtasksFinished();

        assertTrue(enterEvent.defaultPrevented);
        assertFalse(isSuggestionsOpen());
        assertEquals('', combobox.value);
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));
      });

  test(
      'keyboard navigation: Enter when collapsed does not prevent default' +
          ' or stop propagation',
      async () => {
        combobox.options = [{value: 'first'}];
        await microtasksFinished();
        assertFalse(isSuggestionsOpen());

        let hostKeydownFired = false;
        combobox.addEventListener('keydown', () => {
          hostKeydownFired = true;
        });

        const enterEvent = new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        combobox.inputElement.dispatchEvent(enterEvent);
        await microtasksFinished();

        assertFalse(enterEvent.defaultPrevented);
        assertTrue(hostKeydownFired);
      });

  test(
      'keyboard navigation: Enter when expanded stops propagation',
      async () => {
        combobox.options = [{value: 'first'}, {value: 'second'}];
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());

        let hostKeydownFired = false;
        combobox.addEventListener('keydown', () => {
          hostKeydownFired = true;
        });

        combobox.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          cancelable: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(hostKeydownFired);
      });

  test('keyboard navigation: Escape closes suggestions', async () => {
    combobox.options = [{value: 'item'}];
    await microtasksFinished();

    combobox.inputElement.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
    await microtasksFinished();

    assertTrue(isSuggestionsOpen());

    combobox.inputElement.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    await microtasksFinished();

    assertFalse(isSuggestionsOpen());
  });

  test(
      'keyboard navigation: Space in readonly mode opens and selects',
      async () => {
        combobox.readonly = true;
        combobox.options = [{value: 'one'}, {value: 'two'}];
        await microtasksFinished();

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: ' ', bubbles: true}));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();

        const valueChangedPromise =
            eventToPromise<CustomEvent<{value: string}>>(
                'value-changed', combobox);
        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: ' ', bubbles: true}));
        const event = await valueChangedPromise;
        await microtasksFinished();

        assertEquals('two', event.detail.value);
        assertEquals('two', combobox.value);
        assertFalse(isSuggestionsOpen());
      });

  test(
      'keyboard navigation: Space in editable mode does not select option',
      async () => {
        combobox.readonly = false;
        combobox.options = [{value: 'alpha'}, {value: 'beta'}];
        await microtasksFinished();

        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();

        assertTrue(isSuggestionsOpen());
        const items = getSuggestionItems();
        assertTrue(items[0]!.classList.contains('highlighted'));

        const spaceEvent = new KeyboardEvent(
            'keydown', {key: ' ', bubbles: true, cancelable: true});
        combobox.inputElement.dispatchEvent(spaceEvent);
        await microtasksFinished();

        assertFalse(spaceEvent.defaultPrevented);
        assertEquals('', combobox.value);
        assertTrue(isSuggestionsOpen());
      });

  test(
      'errorMessage renders error container and closes suggestions',
      async () => {
        combobox.options = [{value: 'one'}];
        await microtasksFinished();

        getDropdownButton()!.dispatchEvent(
            new PointerEvent('pointerdown', {bubbles: true}));
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());
        assertTrue(combobox.inputElement.hasAttribute('aria-activedescendant'));

        combobox.errorMessage = 'Required field';
        await microtasksFinished();

        const error = combobox.shadowRoot.querySelector('#error');
        assertTrue(!!error);
        assertEquals('Required field', error.textContent.trim());
        assertEquals(
            'true', combobox.inputElement.getAttribute('aria-invalid'));
        assertEquals(
            'error', combobox.inputElement.getAttribute('aria-describedby'));
        assertFalse(isSuggestionsOpen());
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertFalse(isSuggestionsOpen());
      });

  test('removing combobox from DOM closes open popover safely', async () => {
    combobox.options = [{value: 'entry'}];
    await microtasksFinished();

    getDropdownButton()!.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    const suggestions = getSuggestions();
    assertTrue(!!suggestions);
    assertTrue(suggestions.matches(':popover-open'));

    document.body.removeChild(combobox);
    assertFalse(suggestions.matches(':popover-open'));
  });

  test(
      'blurring input closes suggestions, resets highlight and filter query',
      async () => {
        combobox.options =
            [{value: 'first'}, {value: 'second'}, {value: 'third'}];
        await microtasksFinished();

        // Focus the input to open suggestions.
        combobox.focus();
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());
        assertEquals(
            'true', combobox.inputElement.getAttribute('aria-expanded'));

        // Navigate with ArrowDown to highlight an item.
        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        assertEquals(
            'suggestion-0',
            combobox.inputElement.getAttribute('aria-activedescendant'));

        // Type to filter options.
        combobox.inputElement.value = 'fir';
        combobox.inputElement.dispatchEvent(
            new Event('input', {bubbles: true}));
        await microtasksFinished();
        assertEquals(1, getSuggestionItems().length);

        // Blurring the input should close suggestions, clear highlight,
        // and reset the filter query.
        combobox.inputElement.dispatchEvent(new Event('blur'));
        await microtasksFinished();

        assertFalse(isSuggestionsOpen());
        assertEquals(
            'false', combobox.inputElement.getAttribute('aria-expanded'));
        assertFalse(
            combobox.inputElement.hasAttribute('aria-activedescendant'));

        // Re-opening suggestions shows all options because filter query
        // was reset.
        getDropdownButton()!.dispatchEvent(
            new PointerEvent('pointerdown', {bubbles: true}));
        await microtasksFinished();
        assertTrue(isSuggestionsOpen());
        assertEquals(3, getSuggestionItems().length);
      });

  test(
      'pointerdown on suggestions container prevents default to retain focus',
      async () => {
        combobox.options = [{value: 'opt1'}];
        await microtasksFinished();

        getDropdownButton()!.dispatchEvent(
            new PointerEvent('pointerdown', {bubbles: true}));
        await microtasksFinished();

        const suggestions = getSuggestions();
        assertTrue(!!suggestions);

        const pointerDownEvent =
            new PointerEvent('pointerdown', {bubbles: true, cancelable: true});
        suggestions.dispatchEvent(pointerDownEvent);
        await microtasksFinished();

        assertTrue(pointerDownEvent.defaultPrevented);
      });

  test(
      'does not call togglePopover when unrelated properties change',
      async () => {
        combobox.options = [{value: 'opt1'}];
        await microtasksFinished();

        const suggestions = getSuggestions();
        assertTrue(!!suggestions);

        let toggleCount = 0;
        const origToggle = suggestions.togglePopover.bind(suggestions);
        suggestions.togglePopover = (force?: boolean) => {
          toggleCount++;
          return origToggle(force);
        };

        // Changing unrelated properties like label or placeholder should not
        // invoke togglePopover.
        combobox.label = 'New Label';
        await microtasksFinished();
        combobox.placeholder = 'New Placeholder';
        await microtasksFinished();
        assertEquals(0, toggleCount);

        // State affecting dropdown visibility should invoke togglePopover.
        combobox.inputElement.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertEquals(1, toggleCount);
        assertTrue(isSuggestionsOpen());

        // Keyboard navigation changes highlighted index, but visibility doesn't
        // change, so togglePopover should not be called again.
        combobox.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
        await microtasksFinished();
        assertEquals(1, toggleCount);

        // Blurring closes the dropdown, which changes visibility state.
        combobox.inputElement.dispatchEvent(new Event('blur'));
        await microtasksFinished();
        assertEquals(2, toggleCount);
        assertFalse(isSuggestionsOpen());
      });
});
