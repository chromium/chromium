// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksPageState, NodeMap} from 'chrome://bookmarks/bookmarks.js';
import {createEmptyState, removeBookmark, Store, StoreClientMixinLit} from 'chrome://bookmarks/bookmarks.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, testTree} from './test_util.js';

const TestStoreClientBase = StoreClientMixinLit(CrLitElement);

class TestStoreClient extends TestStoreClientBase {
  static get is() {
    return 'test-store-client';
  }

  override render() {
    return html`
      ${this.toArray(this.items).map(item => html`
        <div class="item">${item}</div>`)}
    `;
  }

  static override get properties() {
    return {
      items: {type: Object},
    };
  }

  accessor items: NodeMap = {};
  hasChanged: boolean = false;

  toArray(items: NodeMap) {
    return Object.values(items).map(value => value.id);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateFromStore();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('items') &&
        Object.values(this.items).length > 0) {
      this.itemsChanged(this.items, changedProperties.get('items'));
    }
  }

  override onStateChanged(state: BookmarksPageState) {
    this.items = state.nodes || {};
  }

  itemsChanged(_newItems: NodeMap, oldItems?: NodeMap) {
    if (oldItems && Object.values(oldItems).length > 0) {
      this.hasChanged = true;
    }
  }
}

customElements.define('test-store-client', TestStoreClient);

suite('bookmarks.Store', function() {
  let store: TestStore;

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('11'),
      createItem('12'),
      createItem('13'),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
    });
    store.setReducersEnabled(true);
    store.replaceSingleton();
  });

  test('batch mode disables updates', function() {
    let lastStateChange: BookmarksPageState|null = null;
    const observer = {
      onStateChanged: function(state: BookmarksPageState) {
        lastStateChange = state;
      },
    };

    store.addObserver(observer);
    store.beginBatchUpdate();

    store.dispatch(removeBookmark('11', '1', 0, store.data.nodes));
    assertEquals(null, lastStateChange);
    store.dispatch(removeBookmark('12', '1', 0, store.data.nodes));
    assertEquals(null, lastStateChange);

    store.endBatchUpdate();
    assertDeepEquals(['13'], lastStateChange!.nodes['1']!.children);
  });
});

suite('bookmarks.StoreClientMixin', function() {
  let store: Store;
  let client: TestStoreClient;

  function update(newState: BookmarksPageState): Promise<void> {
    store.data = newState;
    store.endBatchUpdate();
    return microtasksFinished();
  }

  function getRenderedItems() {
    return Array.from(client.shadowRoot.querySelectorAll('.item'))
        .map((div) => div.textContent.trim());
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Reset store instance:
    Store.setInstance(new Store());
    store = Store.getInstance();
    const state = createEmptyState();
    state.nodes = testTree(createFolder('1', [
      createItem('11'),
      createItem('12'),
      createItem('13'),
    ]));
    store.init(state);

    client = document.createElement('test-store-client') as TestStoreClient;
    document.body.appendChild(client);
    return microtasksFinished();
  });

  test('renders initial data', function() {
    assertDeepEquals(['0', '1', '11', '12', '13'], getRenderedItems());
  });

  test('renders changes to watched state', async () => {
    assertFalse(client.hasChanged);
    const newItems = testTree(createFolder('1', [
      createItem('11'),
      createItem('12'),
    ]));
    const newState = Object.assign({}, store.data, {
      nodes: newItems,
    });
    await update(newState);

    assertTrue(client.hasChanged);
    assertDeepEquals(['0', '1', '11', '12'], getRenderedItems());
  });

  test('ignores changes to other subtrees', async () => {
    const newState = Object.assign({}, store.data, {selectedFolder: 'foo'});
    await update(newState);

    assertFalse(client.hasChanged);
  });
});
