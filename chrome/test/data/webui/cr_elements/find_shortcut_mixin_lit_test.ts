// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {FindShortcutManager} from 'chrome://resources/cr_elements/find_shortcut_manager.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {html, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('find-shortcut-lit', () => {
  let wait: PromiseResolver<{modalContextOpen: boolean, self: HTMLElement}>;
  let resolved: boolean;

  function pressCtrlF() {
    pressAndReleaseKeyOn(
        document.documentElement, 70, isMac ? 'meta' : 'ctrl', 'f');
  }

  function pressSlash() {
    pressAndReleaseKeyOn(document.documentElement, 191, [], '/');
  }

  /**
   * Checks that the handleFindShortcut method is being called for the
   * element reference |expectedSelf| when a find shortcut is invoked.
   */
  async function check(
      expectedSelf: HTMLElement, expectedModalContextOpen: boolean = false,
      pressShortcut: () => void = pressCtrlF) {
    wait = new PromiseResolver();
    resolved = false;
    pressShortcut();
    const args = await wait.promise;
    assertEquals(expectedSelf, args.self);
    assertEquals(!!expectedModalContextOpen, args.modalContextOpen);
  }

  /**
   * Registers for a keydown event to check whether the bubbled up event has
   * defaultPrevented set to true, in which case the event was handled.
   */
  async function listenOnceAndCheckDefaultPrevented(defaultPrevented: boolean) {
    const e = await eventToPromise('keydown', window);
    assertEquals(e.defaultPrevented, defaultPrevented);
  }

  const FindShortcutManualListenLitElementBase =
      FindShortcutMixinLit(CrLitElement);

  class FindShortcutManualListenLitElement extends
      FindShortcutManualListenLitElementBase {
    override render() {
      return html`<div>Hello World</div><slot id="slotted"></slot>`;
    }

    hasFocus: boolean;

    constructor() {
      super();
      this.findShortcutListenOnAttach = false;
      this.hasFocus = false;
    }

    override handleFindShortcut(modalContextOpen: boolean) {
      assertFalse(resolved);
      wait.resolve({modalContextOpen, self: this});
      return true;
    }

    override searchInputHasFocus() {
      return this.hasFocus;
    }
  }
  customElements.define(
      'find-shortcut-manual-listen-lit', FindShortcutManualListenLitElement);

  const FindShortcutLitElementBase = FindShortcutMixinLit(CrLitElement);
  class FindShortcutLitElement extends FindShortcutLitElementBase {
    override render() {
      return html`<div>Hello World</div><slot id="slotted"></slot>`;
    }

    handledResponse: boolean;
    hasFocus: boolean;

    constructor() {
      super();
      this.handledResponse = true;
      this.hasFocus = false;
    }

    override handleFindShortcut(modalContextOpen: boolean) {
      assertFalse(resolved);
      wait.resolve({modalContextOpen, self: this});
      return this.handledResponse;
    }

    override searchInputHasFocus() {
      return this.hasFocus;
    }
  }
  customElements.define('find-shortcut-lit', FindShortcutLitElement);

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertEquals(0, FindShortcutManager.listeners.length);
  });

  test('handled', async () => {
    const testElement = document.createElement('find-shortcut-lit');
    document.body.appendChild(testElement);
    await check(testElement);
  });

  test('handled with modal context open', async () => {
    const testElement = document.createElement('find-shortcut-lit');
    const dialog = document.createElement('cr-dialog');
    document.body.appendChild(testElement);
    document.body.appendChild(dialog);

    dialog.showModal();
    await dialog.updateComplete;
    await check(testElement, true);
  });

  test('handled with modal context closed', async () => {
    const testElement = document.createElement('find-shortcut-lit')!;
    const dialog = document.createElement('cr-dialog');
    document.body.appendChild(testElement);
    document.body.appendChild(dialog);

    dialog.showModal();
    assertTrue(dialog.open);
    const whenCloseFired = eventToPromise('close', dialog);
    dialog.close();
    await whenCloseFired;
    await check(testElement);
  });

  test('last listener is active', async () => {
    const length = 2;
    for (let i = 0; i < length; i++) {
      document.body.appendChild(document.createElement('find-shortcut-lit'));
    }

    assertEquals(length, FindShortcutManager.listeners.length);
    const testElements =
        document.body.querySelectorAll<HTMLElement>('find-shortcut-lit');
    await check(testElements[1]!);
  });

  test('can remove listeners out of order', async () => {
    const length = 4;
    for (let i = 0; i < length; i++) {
      document.body.appendChild(
          document.createElement('find-shortcut-manual-listen-lit'));
    }
    const testElements =
        document.body.querySelectorAll<FindShortcutManualListenLitElement>(
            'find-shortcut-manual-listen-lit');
    testElements[0]!.becomeActiveFindShortcutListener();
    testElements[1]!.becomeActiveFindShortcutListener();
    testElements[0]!.removeSelfAsFindShortcutListener();
    await check(testElements[1]!);
    testElements[1]!.removeSelfAsFindShortcutListener();
  });

  test('removing self when not active throws exception', () => {
    const length = 2;
    for (let i = 0; i < length; i++) {
      document.body.appendChild(
          document.createElement('find-shortcut-manual-listen-lit'));
    }
    const testElement =
        document.body.querySelector<FindShortcutManualListenLitElement>(
            'find-shortcut-manual-listen-lit')!;
    assertThrows(() => testElement.removeSelfAsFindShortcutListener());
  });

  test('throw exception when try to become active already a listener', () => {
    document.body.innerHTML = getTrustedHTML`
        <find-shortcut-lit>
          <find-shortcut-lit></find-shortcut-lit>
        </find-shortcut-lit>`;
    const testElements = document.body.querySelectorAll<FindShortcutLitElement>(
        'find-shortcut-lit');
    assertThrows(() => testElements[0]!.becomeActiveFindShortcutListener());
    assertThrows(() => testElements[1]!.becomeActiveFindShortcutListener());
  });

  test('cmd+ctrl+f bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    document.body.appendChild(document.createElement('find-shortcut-lit'));
    pressAndReleaseKeyOn(document.documentElement, 70, ['meta', 'ctrl'], 'f');
    await bubbledUp;
  });

  test('find shortcut bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(true);
    document.body.appendChild(document.createElement('find-shortcut-lit'));
    const testElement = document.body.querySelector<FindShortcutLitElement>(
        'find-shortcut-lit')!;
    await check(testElement);
    await bubbledUp;
  });

  test('shortcut with no listeners bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    pressAndReleaseKeyOn(
        document.documentElement, 70, isMac ? 'meta' : 'ctrl', 'f');
    await bubbledUp;
  });

  test('inner listener is active when listening on attach', async () => {
    document.body.innerHTML = getTrustedHTML`
        <find-shortcut-lit>
          <find-shortcut-lit></find-shortcut-lit>
        </find-shortcut-lit>`;
    const testElements = document.body.querySelectorAll<FindShortcutLitElement>(
        'find-shortcut-lit');
    assertEquals(2, FindShortcutManager.listeners.length);
    await check(testElements[1]!);
  });

  test('not handle by listener bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    const testElement =
        document.createElement('find-shortcut-lit') as FindShortcutLitElement;
    document.body.appendChild(testElement);
    testElement.handledResponse = false;
    await check(testElement);
    await bubbledUp;
  });

  test('when element has focus, shortcut is handled by next', async () => {
    const length = 3;
    for (let i = 0; i < length; i++) {
      document.body.appendChild(document.createElement('find-shortcut-lit'));
    }
    const testElements =
        Array.from(document.body.querySelectorAll<FindShortcutLitElement>(
            'find-shortcut-lit'));
    testElements[0]!.hasFocus = true;
    await check(testElements[2]!);
    testElements[0]!.hasFocus = false;
    await microtasksFinished();
    testElements[1]!.hasFocus = true;
    await check(testElements[0]!);
    testElements[1]!.hasFocus = false;
    await microtasksFinished();
    testElements[2]!.hasFocus = true;
    await check(testElements[1]!);
  });

  test('slash "/" is supported as a keyboard shortcut', async () => {
    const testElement =
        document.createElement('find-shortcut-lit') as FindShortcutLitElement;
    document.body.appendChild(testElement);
    testElement.hasFocus = false;
    await check(testElement, false, pressSlash);
  });
});
