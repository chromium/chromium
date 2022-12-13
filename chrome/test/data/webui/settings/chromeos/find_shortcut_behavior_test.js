// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {FindShortcutBehavior, FindShortcutManager} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('find-shortcut', () => {
  /** @override */

  /**
   * @type {PromiseResolver<!{modalContextOpen: boolean, self: HTMLElement}>}
   */
  let wait;
  /** @type {boolean} */
  let resolved;

  const pressCtrlF = () =>
      pressAndReleaseKeyOn(window, 70, 'ctrl', 'f');
  const pressSlash = () => pressAndReleaseKeyOn(window, 191, '', '/');

  /**
   * Checks that the handleFindShortcut method is being called for the
   * element reference |expectedSelf| when a find shortcut is invoked.
   * @param {!HTMLElement} expectedSelf
   * @param {boolean} expectedModalContextOpen
   * @param {function()} pressShortcut
   * @return {!Promise}
   */
  const check = async (
      expectedSelf, expectedModalContextOpen = false,
      pressShortcut = pressCtrlF) => {
    wait = new PromiseResolver();
    resolved = false;
    pressShortcut();
    const args = await wait.promise;
    assertEquals(expectedSelf, args.self);
    assertEquals(!!expectedModalContextOpen, args.modalContextOpen);
  };

  /**
   * Registers for a keydown event to check whether the bubbled up event has
   * defaultPrevented set to true, in which case the event was handled.
   * @param {boolean} defaultPrevented
   * @return {!Promise}
   */
  const listenOnceAndCheckDefaultPrevented = async defaultPrevented => {
    const e = await eventToPromise('keydown', window);
    assertEquals(e.defaultPrevented, defaultPrevented);
  };

  suiteSetup(() => {
    Polymer({
      is: 'find-shortcut-element-manual-listen',
      _template: null,

      behaviors: [FindShortcutBehavior],

      findShortcutListenOnAttach: false,
      hasFocus: false,

      handleFindShortcut(modalContextOpen) {
        assert(!resolved);
        wait.resolve({modalContextOpen, self: this});
        return true;
      },

      searchInputHasFocus() {
        return this.hasFocus;
      },
    });

    Polymer({
      is: 'find-shortcut-element',
      _template: null,
      behaviors: [FindShortcutBehavior],

      handledResponse: true,
      hasFocus: false,

      handleFindShortcut(modalContextOpen) {
        assert(!resolved);
        wait.resolve({modalContextOpen, self: this});
        return this.handledResponse;
      },

      searchInputHasFocus() {
        return this.hasFocus;
      },
    });
    PolymerTest.clearBody();
  });

  teardown(() => {
    PolymerTest.clearBody();
    assertEquals(0, FindShortcutManager.listeners.length);
  });

  test('handled', async () => {
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    await check(testElement);
  });

  test('handled with modal context open', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    await check(testElement, true);
  });

  test('handled with modal context closed', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    assertTrue(dialog.open);
    const whenCloseFired = eventToPromise('close', dialog);
    dialog.close();
    await whenCloseFired;
    await check(testElement);
  });

  test('last listener is active', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <find-shortcut-element></find-shortcut-element>`;
    assertEquals(2, FindShortcutManager.listeners.length);
    const testElements =
        document.body.querySelectorAll('find-shortcut-element');
    await check(testElements[1]);
  });

  test('can remove listeners out of order', async () => {
    document.body.innerHTML = `
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>`;
    const testElements =
        document.body.querySelectorAll('find-shortcut-element-manual-listen');
    testElements[0].becomeActiveFindShortcutListener();
    testElements[1].becomeActiveFindShortcutListener();
    testElements[0].removeSelfAsFindShortcutListener();
    await check(testElements[1]);
    testElements[1].removeSelfAsFindShortcutListener();
  });

  test('removing self when not active throws exception', () => {
    document.body.innerHTML = `
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>`;
    const testElement =
        document.body.querySelector('find-shortcut-element-manual-listen');
    assertThrows(() => testElement.removeSelfAsFindShortcutListener());
  });

  test('throw exception when try to become active already a listener', () => {
    document.body.innerHTML = `
        <find-shortcut-element>
          <find-shortcut-element></find-shortcut-element>
        </find-shortcut-element>`;
    const testElements =
        document.body.querySelectorAll('find-shortcut-element');
    assertThrows(() => testElements[0].becomeActiveFindShortcutListener());
    assertThrows(() => testElements[1].becomeActiveFindShortcutListener());
  });

  test('cmd+ctrl+f bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    pressAndReleaseKeyOn(window, 70, ['meta', 'ctrl'], 'f');
    await bubbledUp;
  });

  test('find shortcut bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(true);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    await check(testElement);
    await bubbledUp;
  });

  test('shortcut with no listeners bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    pressAndReleaseKeyOn(window, 70, 'ctrl', 'f');
    await bubbledUp;
  });

  test('inner listener is active when listening on attach', async () => {
    document.body.innerHTML = `
        <find-shortcut-element>
          <find-shortcut-element></find-shortcut-element>
        </find-shortcut-element>`;
    const testElements =
        document.body.querySelectorAll('find-shortcut-element');
    assertEquals(2, FindShortcutManager.listeners.length);
    await check(testElements[1]);
  });

  test('not handle by listener bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    testElement.handledResponse = false;
    await check(testElement);
    await bubbledUp;
  });

  test('when element has focus, shortcut is handled by next', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <find-shortcut-element></find-shortcut-element>
        <find-shortcut-element></find-shortcut-element>`;
    const testElements =
        Array.from(document.body.querySelectorAll('find-shortcut-element'));
    testElements[0].hasFocus = true;
    await check(testElements[2]);
    testElements[0].hasFocus = false;
    testElements[1].hasFocus = true;
    await check(testElements[0]);
    testElements[1].hasFocus = false;
    testElements[2].hasFocus = true;
    await check(testElements[1]);
  });

  test('slash "/" is supported as a keyboard shortcut', async () => {
    document.body.innerHTML = '<find-shortcut-element></find-shortcut-element>';
    const testElement = document.body.querySelector('find-shortcut-element');
    testElement.hasFocus = false;
    await check(testElement, false, pressSlash);
  });
});
