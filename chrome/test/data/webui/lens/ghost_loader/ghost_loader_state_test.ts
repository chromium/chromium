// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/lens/shared/searchbox_ghost_loader.js';

import type {SearchboxGhostLoaderElement} from 'chrome-untrusted://lens/lens/shared/searchbox_ghost_loader.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/lens/shared/searchbox_ghost_loader_browser_proxy.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensGhostLoaderBrowserProxy} from './test_ghost_loader_browser_proxy.js';

suite('SearchboxGhostLoader', () => {
  let testBrowserProxy: TestLensGhostLoaderBrowserProxy;
  let searchboxGhostLoaderElement: SearchboxGhostLoaderElement;

  function getSearchboxGhostLoaderElement(): HTMLElement {
    // The ghost loader loading UI can be replaced by a different UI if
    // LensOverlayVisualSelectionUpdates is enabled. Grab the new UI if it
    // exists, otherwise use the old UI.
    const newLoadingState =
        searchboxGhostLoaderElement.shadowRoot!.querySelector<HTMLElement>(
            ':host([enable-csb-motion-tweaks]) .suggestion-loader-container');
    return newLoadingState ??
        searchboxGhostLoaderElement.shadowRoot!.querySelector<HTMLElement>(
            '#loadingState')!;
  }

  setup(() => {
    testBrowserProxy = new TestLensGhostLoaderBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    searchboxGhostLoaderElement =
        document.createElement('cr-searchbox-ghost-loader');
    document.body.appendChild(searchboxGhostLoaderElement);
    return waitAfterNextRender(searchboxGhostLoaderElement);
  });

  test('verify autocomplete timeout shows error state', async () => {
    // Initially, the ghost loader should show the loading state and have the
    // error state hidden.
    assertFalse(isVisible(
        searchboxGhostLoaderElement.shadowRoot!.querySelector<HTMLElement>(
            '#errorState')));
    assertTrue(isVisible(getSearchboxGhostLoaderElement()));
    testBrowserProxy.page.showErrorState();
    await waitAfterNextRender(searchboxGhostLoaderElement);
    // After autocomplete stop timer has been triggered, the ghost loader
    // should switch to showing the error state.
    assertFalse(isVisible(getSearchboxGhostLoaderElement()));
    assertTrue(isVisible(
        searchboxGhostLoaderElement.shadowRoot!.querySelector<HTMLElement>(
            '#errorState')));
  });
});
