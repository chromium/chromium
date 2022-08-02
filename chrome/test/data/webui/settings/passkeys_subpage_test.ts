// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the passkeys subpage.
 */

import {Passkey, PasskeysBrowserProxy, PasskeysBrowserProxyImpl, SettingsPasskeysSubpageElement} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

class TestPasskeysBrowserProxy extends TestBrowserProxy implements
    PasskeysBrowserProxy {
  constructor() {
    super([
      'enumerate',
      'delete',
    ]);
  }

  // nextPasskeys_ is the next result to return from a call to `enumerate`
  // or `delete`.
  private nextPasskeys_: Passkey[]|null = null;

  setNextPasskeys(passkeys: Passkey[]|null) {
    this.nextPasskeys_ = passkeys;
    this.resetResolver('enumerate');
    this.resetResolver('delete');
  }

  hasPasskeys(): Promise<boolean> {
    return Promise.resolve(true);
  }

  enumerate(): Promise<Passkey[]|null> {
    this.methodCalled('enumerate');
    return this.consumeNext_();
  }

  delete(credentialId: string): Promise<Passkey[]|null> {
    this.methodCalled('delete', credentialId);
    return this.consumeNext_();
  }

  private consumeNext_(): Promise<Passkey[]|null> {
    const result = this.nextPasskeys_;
    this.nextPasskeys_ = null;
    return Promise.resolve(result);
  }
}

/**
 * Get the usernames of the passkeys currently displayed.
 */
function getUsernamesFromList(list: HTMLElement): string[] {
  const inputs = Array.from(list.shadowRoot!.querySelectorAll<HTMLElement>(
      '.list-item .username-column'));
  return inputs.slice(1).map(input => input.textContent!.trim());
}

/**
 * Click the `num`th drop-down icon in the list of passkeys.
 */
function clickDots(page: HTMLElement, num: number) {
  const icon = page.shadowRoot!.querySelectorAll<HTMLElement>(
      '.list-item .icon-more-vert')[num];
  assertTrue(icon !== undefined);
  icon.click();
}

/**
 * Click the button named `name` in the drop-down.
 */
function clickButton(page: HTMLElement, name: string) {
  const menu = page.shadowRoot!.querySelector<HTMLElement>('#menu')!;
  const button = menu.querySelector<HTMLElement>('#' + name);

  assertTrue(button !== null, name + ' button missing');
  if (button === null) {
    return;
  }

  button.click();
}

suite('PasskeysSubpage', function() {
  let browserProxy: TestPasskeysBrowserProxy;
  let page: SettingsPasskeysSubpageElement;

  setup(async function() {
    browserProxy = new TestPasskeysBrowserProxy();
    PasskeysBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-passkeys-subpage');
  });

  test('NoSupport', async function() {
    browserProxy.setNextPasskeys(null);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    const shown = getUsernamesFromList(page);
    assertEquals(shown.length, 0, 'No passkeys shown');

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#error') !== null,
        'Error message shown');
  });

  test('Credentials', async function() {
    const passkeys = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid1',
        userName: 'user1',
        userDisplayName: 'displayName1',
      },
      {
        credentialId: '2',
        relyingPartyId: 'rpid2',
        userName: 'user2',
        userDisplayName: 'displayName2',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#error') === null,
        'Error message not shown');

    assertDeepEquals(
        getUsernamesFromList(page), passkeys.map(cred => cred.userName));
  });

  test('Delete', async function() {
    const passkeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid_x',
        userName: 'user',
        userDisplayName: 'displayName',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#error') === null,
        'Error message not shown');
    assertDeepEquals(getUsernamesFromList(page), [passkeys[0].userName]);

    clickDots(page, 0);

    browserProxy.whenCalled('delete').then((name: string) => {
      assertEquals(name, passkeys[0].credentialId);
    });
    browserProxy.setNextPasskeys([]);
    clickButton(page, 'delete');
    await flushTasks();
    assertEquals(browserProxy.getCallCount('delete'), 1);

    assertDeepEquals(getUsernamesFromList(page), []);
  });
});
