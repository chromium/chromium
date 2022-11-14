// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {removeBookmark, Store, StoreClientMixin} from 'chrome://bookmarks/bookmarks.js';
import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('bookmarks.Store', function() {
  let store;

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
    let lastStateChange = null;
    const observer = {
      onStateChanged: function(state) {
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
    assertDeepEquals(['13'], lastStateChange.nodes['1'].children);
  });
});

suite('bookmarks.StoreClientMixin', function() {
  let store;
  let client;

  function update(newState) {
    store.notifyObservers_(newState);
    flush();
  }

  function getRenderedItems() {
    return Array.from(client.root.querySelectorAll('.item'))
        .map((div) => div.textContent.trim());
  }

  suiteSetup(function() {
    const TestStoreClientBase = StoreClientMixin(PolymerElement);
    class TestStoreClient extends TestStoreClientBase {
      static get template() {
        return html`
          <template is="dom-repeat" items="[[items]]">
            <div class="item">[[item]]</div>
          </template>
        `;
      }

      static get properties() {
        return {
          items: {
            type: Array,
            observer: 'itemsChanged_',
          },
        };
      }

      connectedCallback() {
        super.connectedCallback();
        this.hasChanged = false;
        this.watch('items', function(state) {
          return state.items;
        });
        this.updateFromStore();
      }

      itemsChanged_(newItems, oldItems) {
        if (oldItems) {
          this.hasChanged = true;
        }
      }
    }

    customElements.define('test-store-client', TestStoreClient);
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;

    // Reset store instance:
    Store.setInstance(new Store());
    store = Store.getInstance();
    store.init({
      items: ['apple', 'banana', 'cantaloupe'],
      count: 3,
    });

    client = document.createElement('test-store-client');
    document.body.appendChild(client);
    flush();
  });

  test('renders initial data', function() {
    assertDeepEquals(['apple', 'banana', 'cantaloupe'], getRenderedItems());
  });

  test('renders changes to watched state', function() {
    const newItems = ['apple', 'banana', 'courgette', 'durian'];
    const newState = Object.assign({}, store.data, {
      items: newItems,
    });
    update(newState);

    assertTrue(client.hasChanged);
    assertDeepEquals(newItems, getRenderedItems());
  });

  test('ignores changes to other subtrees', function() {
    const newState = Object.assign({}, store.data, {count: 2});
    update(newState);

    assertFalse(client.hasChanged);
  });
});
