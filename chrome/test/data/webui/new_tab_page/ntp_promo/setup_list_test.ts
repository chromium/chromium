// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import './ntp_promo_test_common.js';

import type {SetupListElement, SetupListItemElement, SetupListModuleWrapperElement} from 'chrome://new-tab-page/lazy_load.js';
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
  // Test promos. The number of promos is not important, since slices or limits
  // are set by tests as needed.
  for (let i = 0; i < 10; ++i) {
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

  setup(() => {
    testProxy = TestNtpPromoProxy.install();

    document.body.innerHTML = getTrustedHTML`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='bodyText'>This is some body text</p>
    </div>`;

    setupListWrapper = document.createElement('setup-list-module-wrapper');
    setupListWrapper.id = 'setupListWrapper';
    setupListWrapper.maxPromos = 10;
    setupListWrapper.maxCompletedPromos = 2;
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

  test('eligible then completed', async () => {
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
    assertEquals(promos[0]!.buttonText, setupList.$.backing.ariaLabel);

    // Then completed promos.
    setupList = getPromoAt(1);
    assertTrue(setupList.completed);
    assertEquals(promos[1]!.bodyText, setupList.$.bodyText.innerText);
    assertNull(setupList.querySelector('#actionIcon'));
  });

  test('promo count limits', async () => {
    const limit = 3;
    const completed_limit = 1;
    assertTrue(limit < promos.length);
    getSetupList().maxPromos = limit;
    getSetupList().maxCompletedPromos = completed_limit;
    getSetupList().onSetPromos(promos, promos);
    await waitForVisibilityEvents();
    assertEquals(getPromoCount(), limit, 'promo count');
    for (let i = 0; i < limit; i++) {
      assertEquals(
          getPromoAt(i).completed, (i >= (limit - completed_limit)),
          'promo completed');
    }
  });

  test('completed promo is disabled', async () => {
    getSetupList().onSetPromos([], promos);
    await waitForVisibilityEvents();
    assertTrue((getPromoAt(0).$.backing as HTMLButtonElement).disabled);
  });
});
