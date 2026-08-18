// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputMixin} from 'chrome://print/print_preview.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('InputMixinTest', function() {
  let testInput: TestInputElement;

  const TestInputElementBase = InputMixin(PolymerElement);

  class TestInputElement extends TestInputElementBase {
    static get is() {
      return 'test-input';
    }

    static get template() {
      return html`
        <input id="input" data-timeout-delay="50">
      `;
    }

    inputChanges: string[] = [];

    override getInput(): HTMLInputElement {
      return this.shadowRoot!.querySelector<HTMLInputElement>('#input')!;
    }

    override connectedCallback() {
      super.connectedCallback();
      this.addEventListener('input-change', (e: CustomEvent<string>) => {
        this.inputChanges.push(e.detail);
      });
    }
  }

  customElements.define(TestInputElement.is, TestInputElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testInput = document.createElement('test-input') as TestInputElement;
    document.body.appendChild(testInput);
  });

  // Tests that input-change is dispatched after timeout when the user types.
  test('dispatches input-change after timeout', async () => {
    const input = testInput.getInput();
    input.value = 'hello';
    input.dispatchEvent(new CustomEvent('input'));

    const event = await eventToPromise('input-change', testInput);
    assertEquals('hello', event.detail);
    assertEquals(1, testInput.inputChanges.length);
    assertEquals('hello', testInput.inputChanges[0]);
  });

  // Tests that rapid input events are debounced.
  test('debounces rapid input events', async () => {
    const input = testInput.getInput();
    input.value = 'h';
    input.dispatchEvent(new CustomEvent('input'));
    input.value = 'he';
    input.dispatchEvent(new CustomEvent('input'));
    input.value = 'hel';
    input.dispatchEvent(new CustomEvent('input'));

    const event = await eventToPromise('input-change', testInput);
    assertEquals('hel', event.detail);
    assertEquals(1, testInput.inputChanges.length);
  });

  // Tests that Enter or Tab keydown immediately triggers the update.
  test('triggers update on enter or tab keydown', () => {
    const input = testInput.getInput();
    input.value = 'immediate';
    input.dispatchEvent(new CustomEvent('input'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertEquals(1, testInput.inputChanges.length);
    assertEquals('immediate', testInput.inputChanges[0]);
  });

  // Tests that unchanged values do not dispatch input-change.
  test('does not dispatch if value is unchanged', async () => {
    const input = testInput.getInput();
    input.value = 'same';
    input.dispatchEvent(new CustomEvent('input'));
    await eventToPromise('input-change', testInput);
    assertEquals(1, testInput.inputChanges.length);

    // Fire input event with the exact same value.
    input.dispatchEvent(new CustomEvent('input'));
    await new Promise(resolve => setTimeout(resolve, 80));
    assertEquals(1, testInput.inputChanges.length);
  });

  // Tests that resetString() synchronizes lastValue_ with the input element's
  // current value after programmatic changes.
  test('resetString synchronizes value after programmatic change', async () => {
    const input = testInput.getInput();

    // Programmatically update the input value and call resetString().
    input.value = 'programmatic';
    testInput.resetString();

    // No input-change event should have been dispatched.
    assertEquals(0, testInput.inputChanges.length);

    // User now types a new value.
    input.value = 'programmatic-updated';
    input.dispatchEvent(new CustomEvent('input'));

    const event = await eventToPromise('input-change', testInput);
    assertEquals('programmatic-updated', event.detail);
    assertEquals(1, testInput.inputChanges.length);
  });
});
