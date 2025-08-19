// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import './ntp_promo_test_common.js';

import type {SetupListElement, SetupListItemElement, SetupListModuleWrapperElement} from 'chrome://new-tab-page/lazy_load.js';
import {MAX_SETUP_LIST_ENTRIES} from 'chrome://new-tab-page/lazy_load.js';
import {getTrustedHTML} from 'chrome://new-tab-page/new_tab_page.js';
import type {Promo} from 'chrome://new-tab-page/ntp_promo.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestNtpPromoProxy} from './ntp_promo_test_common.js';

suite('SetupListTest', () => {
  let testProxy: TestNtpPromoProxy;
  let setupListWrapper: SetupListModuleWrapperElement;
  const promos: Promo[] = [];
  for (let i = 0; i < MAX_SETUP_LIST_ENTRIES + 1; ++i) {
    promos.push({
      id: 'promo' + i,
      iconName: '',
      bodyText: 'body text ' + i,
      buttonText: 'button text ' + i,
    });
  }

  /**
   * Waits for the current frame to render, which queues intersection events,
   * and then waits for the intersection events to propagate to listeners, which
   * triggers visibility messages.
   *
   * This takes a total of two frames. A single frame will cause the layout to
   * be updated, but will not actually propagate the events.
   */
  async function waitForVisibilityEvents() {
    await waitAfterNextRender(setupListWrapper);
    return waitAfterNextRender(setupListWrapper);
  }

  function getSetupList(): SetupListElement {
    return setupListWrapper.$.setupList;
  }

  function getPromoCount(): number {
    return getSetupList().$.promos.childElementCount;
  }

  function getPromoAt(index: number): SetupListItemElement {
    return getSetupList().$.promos.children[index] as SetupListItemElement;
  }

  function ids(start: number, length: number): string[] {
    return promos.slice(start, start + length).map(promo => promo.id);
  }

  setup(() => {
    testProxy = TestNtpPromoProxy.install();

    document.body.innerHTML = getTrustedHTML`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='bodyText'>This is some body text</p>
    </div>`;

    setupListWrapper = document.createElement('setup-list-module-wrapper');
    setupListWrapper.id = 'setupListWrapper';
    document.querySelector<HTMLElement>('#container')!.insertBefore(
        setupListWrapper, document.querySelector<HTMLElement>('#bodyText'));

    return waitForVisibilityEvents();
  });

  test('promo starts invisible', () => {
    assertFalse(isVisible(setupListWrapper), 'promo should not start visible');
  });

  test('set no promos hides promo', async () => {
    getSetupList().onSetPromos([], []);
    await waitForVisibilityEvents();
    assertFalse(isVisible(setupListWrapper));
  });

  test('set eligible and completed', async () => {
    getSetupList().onSetPromos([promos[0]!], [promos[1]!]);
    await waitForVisibilityEvents();
    assertTrue(isVisible(getSetupList()), 'promo frame should become visible');
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    const expected = [[[promos[0]!.id], [promos[1]!.id]]];
    const actual = testProxy.getHandler().getArgs('onPromosShown');
    assertDeepEquals(expected, actual);
    assertEquals(2, getPromoCount());

    // Pending promos come first.
    let setupList = getPromoAt(0);
    assertFalse(setupList.completed);
    assertEquals(promos[0]!.bodyText, setupList.$.bodyText.innerText);
    assertEquals(promos[0]!.buttonText, setupList.$.actionButton.ariaLabel);

    // Then completed promos.
    setupList = getPromoAt(1);
    assertTrue(setupList.completed);
    assertEquals(promos[1]!.bodyText, setupList.$.bodyText.innerText);
    assertNull(setupList.querySelector('#actionButton'));

  });

  test('set too many promos', async () => {
    getSetupList().onSetPromos(promos, []);
    await waitForVisibilityEvents();
    assertEquals(1, testProxy.getHandler().getCallCount('onPromosShown'));
    assertDeepEquals(
        [[ids(0, MAX_SETUP_LIST_ENTRIES), []]],
        testProxy.getHandler().getArgs('onPromosShown'));
    assertEquals(MAX_SETUP_LIST_ENTRIES, getPromoCount());

    // Check the promos are in order:
    for (let i = 0; i < MAX_SETUP_LIST_ENTRIES; ++i) {
      assertFalse(getPromoAt(i).completed);
      assertEquals(promos[i]!.bodyText, getPromoAt(i).$.bodyText.innerText);
      assertEquals(
          promos[i]!.buttonText, getPromoAt(i).$.actionButton.ariaLabel);
    }
  });
});
