// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {makeStoreClientMixin} from 'chrome://resources/cr_elements/store_client/store_client.js';
import type {Action} from 'chrome://resources/js/store.js';
import {Store} from 'chrome://resources/js/store.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

interface TestState {
  value: number;
}

interface TestActions extends Action {
  name: 'increment'|'decrement';
}

function reducer({value}: TestState, {name}: TestActions): TestState {
  switch (name) {
    case 'increment':
      return {value: value + 1};
    case 'decrement':
      return {value: value - 1};
  }
}

let store: Store<TestState, TestActions>|null = null;

const TestStoreClientMixin = makeStoreClientMixin(() => {
  return store || (store = new Store({value: 0}, reducer));
});

interface StoreClientTestElement {
  $: {
    value: HTMLElement,
    neverTwo: HTMLElement,
  };
}

class StoreClientTestElement extends TestStoreClientMixin
(PolymerElement) {
  static get is() {
    return 'store-client-test-element';
  }

  static get properties() {
    return {
      value_: Number,
      neverTwo_: Number,
    };
  }

  static get template() {
    return html`<div>
      <span id="value">[[value_]]</span>
      <span id="neverTwo">[[neverTwo_]]</span>
    </div>`;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<number>('value_', state => state.value);
    this.watch<number>(
        'neverTwo_', state => state.value === 2 ? undefined : state.value);
    this.updateFromStore();
  }
}

customElements.define(StoreClientTestElement.is, StoreClientTestElement);

declare global {
  interface HTMLElementTagNameMap {
    'store-client-test-element': StoreClientTestElement;
  }
}

suite('StoreClient', function() {
  let storeClientTestElement: StoreClientTestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    storeClientTestElement =
        document.createElement('store-client-test-element');
    storeClientTestElement.getStore().init({value: 0});
    document.body.appendChild(storeClientTestElement);
  });

  teardown(function() {
    store = null;
  });

  test('displays updated values from store', function() {
    assertEquals(
        '0', storeClientTestElement.$.value.textContent,
        'initial value is displayed');

    storeClientTestElement.dispatch({name: 'increment'});

    assertEquals(
        '1', storeClientTestElement.$.value.textContent,
        'new value is displayed');

    storeClientTestElement.dispatch({name: 'decrement'});
    storeClientTestElement.dispatch({name: 'decrement'});

    assertEquals(
        '-1', storeClientTestElement.$.value.textContent,
        'new value is displayed again');
  });

  test('does not update property if getter returns undefined', function() {
    storeClientTestElement.dispatch({name: 'increment'});
    storeClientTestElement.dispatch({name: 'increment'});
    assertEquals(
        '2', storeClientTestElement.$.value.textContent, '2 is displayed');
    assertEquals(
        '1', storeClientTestElement.$.neverTwo.textContent,
        'old value is displayed because local property was not updated to 2');
  });

  test('observes store while active', function() {
    const store = storeClientTestElement.getStore();
    assertTrue(
        store.hasObserver(storeClientTestElement),
        'element is observing store');
    storeClientTestElement.remove();
    assertFalse(
        store.hasObserver(storeClientTestElement),
        'element is not observing store after remove');
  });
});
