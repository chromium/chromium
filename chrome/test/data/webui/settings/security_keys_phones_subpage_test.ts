// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Phone as a Security Key settings page.
 */

import type {CrInputElement, SecurityKeysPhone, SecurityKeysPhonesBrowserProxy, SecurityKeysPhonesList, SecurityKeysPhonesSubpageElement} from 'chrome://settings/lazy_load.js';
import {SecurityKeysPhonesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestSecurityKeysPhonesBrowserProxy extends TestBrowserProxy implements
    SecurityKeysPhonesBrowserProxy {
  constructor() {
    super([
      'enumerate',
      'delete',
      'rename',
    ]);
  }

  // nextPhonesList_ is the next result to return from a call to `enumerate` or
  // `delete`.
  private nextPhonesList_: SecurityKeysPhonesList|null = null;

  setNextPhonesList(syncedPhones: string[], linkedPhones: string[]) {
    this.nextPhonesList_ =
        [syncedPhones.map(this.toPhone_), linkedPhones.map(this.toPhone_)];
    this.resetResolver('enumerate');
    this.resetResolver('delete');
  }

  /* override */ enumerate(): Promise<SecurityKeysPhonesList> {
    this.methodCalled('enumerate');
    return this.consumeNextPhonesList_();
  }

  /* override */ delete(publicKey: string): Promise<SecurityKeysPhonesList> {
    this.methodCalled('delete', publicKey);
    return this.consumeNextPhonesList_();
  }

  /* override */ rename(publicKey: string, newName: string): Promise<void> {
    this.methodCalled('rename', publicKey, newName);
    return Promise.resolve();
  }

  private consumeNextPhonesList_(): Promise<SecurityKeysPhonesList> {
    const result = this.nextPhonesList_;
    this.nextPhonesList_ = null;
    assertTrue(
        result !== null,
        'browserProxy methods called without result have being set for it');
    return Promise.resolve(result!);
  }

  /**
   * Create a dummy phone given a name.
   */
  private toPhone_(name: string): SecurityKeysPhone {
    return {name, publicKey: name};
  }
}

type ShownPhones = Array<{name: string, hasDots: boolean}>;

/**
 * Get the phones currently listed by the given SecurityKeysPhonesListElement.
 */
function getPhonesFromList(list: HTMLElement): ShownPhones {
  const items = list.shadowRoot!.querySelectorAll('.list-item');
  const ret: ShownPhones = [];

  for (const item of items) {
    const nameSpan = item.querySelector<HTMLElement>('.name-column');
    const dots = item.querySelector<HTMLElement>('.icon-more-vert');

    if (nameSpan != null) {
      ret.push({name: nameSpan.innerText, hasDots: dots !== null});
    }
  }

  return ret;
}

/**
 * Get the list phones currently shown on the page.
 */
function getPhones(page: HTMLElement): [ShownPhones, ShownPhones] {
  return [
    getPhonesFromList(
        page.shadowRoot!.querySelector<HTMLElement>('#syncedPhonesList')!),
    getPhonesFromList(
        page.shadowRoot!.querySelector<HTMLElement>('#linkedPhonesList')!),
  ];
}

/**
 * Click the `num`th drop-down icon in the list of linked phones.
 */
function clickLinkedPhoneDots(page: HTMLElement, num: number) {
  const list =
      page.shadowRoot!.querySelector<HTMLElement>('#linkedPhonesList')!;
  const items = list.shadowRoot!.querySelectorAll('.list-item');
  const dots = items[num]!.querySelector<HTMLElement>('.icon-more-vert')!;
  dots.click();
}

/**
 * Click the button named `name` in the `num`th drop-down.
 */
function clickButton(page: HTMLElement, name: string) {
  const list =
      page.shadowRoot!.querySelector<HTMLElement>('#linkedPhonesList')!;
  const menu = list.shadowRoot!.querySelector<HTMLElement>('#menu')!;
  const button = menu.querySelector<HTMLElement>('#' + name);

  assertTrue(button !== null, name + ' button missing');
  if (button === null) {
    return;
  }

  button.click();
}

suite('SecurityKeysPhonesSubpage', function() {
  let browserProxy: TestSecurityKeysPhonesBrowserProxy;
  let page: SecurityKeysPhonesSubpageElement;

  // These are the initial lists of synced and linked phones that will be
  // displayed when the test starts.
  const initialSynced = ['Synced 1', 'Synced 2'];
  const initialLinked = ['Linked 1', 'Linked 2'];

  setup(async function() {
    browserProxy = new TestSecurityKeysPhonesBrowserProxy();
    SecurityKeysPhonesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('security-keys-phones-subpage');

    browserProxy.setNextPhonesList(initialSynced, initialLinked);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);
  });

  test('Initialization', async function() {
    const shown = getPhones(page);

    // The default entries should be shown.
    assertDeepEquals(shown[0].map(x => x.name), initialSynced);
    assertDeepEquals(shown[1].map(x => x.name), initialLinked);
    // The synced phones should not have the dropdown dots, the linked phones
    // should.
    assertTrue(
        shown[0].every(x => !x.hasDots), 'Synced phones don\'t have dots');
    assertTrue(shown[1].every(x => x.hasDots), 'Linked phones have dots');
  });

  test('Delete', async function() {
    clickLinkedPhoneDots(page, 0);

    // 'delete' should be called for the first linked phone.
    browserProxy.whenCalled('delete').then((name: string) => {
      assertEquals(name, initialLinked[0]);
    });
    browserProxy.setNextPhonesList(initialSynced, ['Linked 2']);
    clickButton(page, 'delete');
    await flushTasks();
    assertEquals(browserProxy.getCallCount('delete'), 1);

    const shown = getPhones(page);
    assertDeepEquals(shown[0].map(x => x.name), initialSynced);
    // The first phone should have disappeared.
    assertDeepEquals(shown[1].map(x => x.name), ['Linked 2']);
  });

  test('Edit', async function() {
    clickLinkedPhoneDots(page, 0);
    clickButton(page, 'edit');

    await flushTasks();

    const dialogs =
        page.shadowRoot!.querySelectorAll('security-keys-phones-dialog');
    assertEquals(dialogs.length, 1, 'num dialogs');
    const dialog = dialogs[0]!;

    const name = dialog.shadowRoot!.querySelector<CrInputElement>('#name')!;
    assertEquals(name.value, 'Linked 1');
    const newName = 'New name';
    name.value = newName;

    const saveButton =
        dialog.shadowRoot!.querySelector<HTMLElement>('#actionButton')!;
    browserProxy.whenCalled('rename').then(
        ([publicKey, requestedName]: [string, string]) => {
          assertEquals(publicKey, initialLinked[0]);
          assertEquals(requestedName, newName);
        });
    browserProxy.setNextPhonesList(initialSynced, [newName, 'Linked 2']);
    saveButton.click();

    await flushTasks();
    assertEquals(browserProxy.getCallCount('rename'), 1, 'rename not called');

    await browserProxy.whenCalled('enumerate');
    await flushTasks();
    assertEquals(
        browserProxy.getCallCount('enumerate'), 1, 'enumerate not called');
    const shown = getPhones(page);

    assertDeepEquals(shown[0].map(x => x.name), initialSynced);
    // The first phone should have been renamed.
    assertDeepEquals(shown[1].map(x => x.name), [newName, 'Linked 2']);
  });
});
