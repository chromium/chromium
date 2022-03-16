// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {FindShortcutBehavior, FindShortcutManager} from 'chrome://resources/cr_elements/find_shortcut_behavior.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('find-shortcut', () => {
  let wait: PromiseResolver<{modalContextOpen: boolean, self: HTMLElement}>;
  let resolved: boolean;

  function pressCtrlF() {
    pressAndReleaseKeyOn(
        document.documentElement, 70, isMac ? 'meta' : 'ctrl', 'f');
  }

  function pressSlash() {
    pressAndReleaseKeyOn(document.documentElement, 191, '', '/');
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

  const FindShortcutManualListenElementBase =
      mixinBehaviors([FindShortcutBehavior], PolymerElement) as
      {new (): PolymerElement & FindShortcutBehavior};

  class FindShortcutManualListenElement extends
      FindShortcutManualListenElementBase {
    static get template() {
      return html`<template></template>`;
    }

    hasFocus: boolean;

    constructor() {
      super();
      this.findShortcutListenOnAttach = false;
      this.hasFocus = false;
    }

    override handleFindShortcut(modalContextOpen: boolean) {
      assert(!resolved);
      wait.resolve({modalContextOpen, self: this});
      return true;
    }

    override searchInputHasFocus() {
      return this.hasFocus;
    }
  }
  customElements.define(
      'find-shortcut-element-manual-listen', FindShortcutManualListenElement);

  const FindShortcutElementBase =
      mixinBehaviors([FindShortcutBehavior], PolymerElement) as
      {new (): PolymerElement & FindShortcutBehavior};

  class FindShortcutElement extends FindShortcutElementBase {
    static get template() {
      return html`<template></template>`;
    }

    handledResponse: boolean;
    hasFocus: boolean;

    constructor() {
      super();
      this.handledResponse = true;
      this.hasFocus = false;
    }

    override handleFindShortcut(modalContextOpen: boolean) {
      assert(!resolved);
      wait.resolve({modalContextOpen, self: this});
      return this.handledResponse;
    }

    override searchInputHasFocus() {
      return this.hasFocus;
    }
  }
  customElements.define('find-shortcut-element', FindShortcutElement);

  suiteSetup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    document.body.innerHTML = '';
    assertEquals(0, FindShortcutManager.listeners.length);
  });

  test('handled', async () => {
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
    await check(testElement);
  });

  test('handled with modal context open', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();
    await check(testElement, true);
  });

  test('handled with modal context closed', async () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
    const dialog = document.body.querySelector('cr-dialog')!;
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
        document.body.querySelectorAll<HTMLElement>('find-shortcut-element');
    await check(testElements[1]!);
  });

  test('can remove listeners out of order', async () => {
    document.body.innerHTML = `
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>`;
    const testElements =
        document.body.querySelectorAll<FindShortcutManualListenElement>(
            'find-shortcut-element-manual-listen');
    testElements[0]!.becomeActiveFindShortcutListener();
    testElements[1]!.becomeActiveFindShortcutListener();
    testElements[0]!.removeSelfAsFindShortcutListener();
    await check(testElements[1]!);
    testElements[1]!.removeSelfAsFindShortcutListener();
  });

  test('removing self when not active throws exception', () => {
    document.body.innerHTML = `
        <find-shortcut-element-manual-listen>
        </find-shortcut-element-manual-listen>`;
    const testElement =
        document.body.querySelector<FindShortcutManualListenElement>(
            'find-shortcut-element-manual-listen')!;
    assertThrows(() => testElement.removeSelfAsFindShortcutListener());
  });

  test('throw exception when try to become active already a listener', () => {
    document.body.innerHTML = `
        <find-shortcut-element>
          <find-shortcut-element></find-shortcut-element>
        </find-shortcut-element>`;
    const testElements = document.body.querySelectorAll<FindShortcutElement>(
        'find-shortcut-element');
    assertThrows(() => testElements[0]!.becomeActiveFindShortcutListener());
    assertThrows(() => testElements[1]!.becomeActiveFindShortcutListener());
  });

  test('cmd+ctrl+f bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    pressAndReleaseKeyOn(document.documentElement, 70, ['meta', 'ctrl'], 'f');
    await bubbledUp;
  });

  test('find shortcut bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(true);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
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
    document.body.innerHTML = `
        <find-shortcut-element>
          <find-shortcut-element></find-shortcut-element>
        </find-shortcut-element>`;
    const testElements = document.body.querySelectorAll<FindShortcutElement>(
        'find-shortcut-element');
    assertEquals(2, FindShortcutManager.listeners.length);
    await check(testElements[1]!);
  });

  test('not handle by listener bubbles up', async () => {
    const bubbledUp = listenOnceAndCheckDefaultPrevented(false);
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
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
        Array.from(document.body.querySelectorAll<FindShortcutElement>(
            'find-shortcut-element'));
    testElements[0]!.hasFocus = true;
    await check(testElements[2]!);
    testElements[0]!.hasFocus = false;
    testElements[1]!.hasFocus = true;
    await check(testElements[0]!);
    testElements[1]!.hasFocus = false;
    testElements[2]!.hasFocus = true;
    await check(testElements[1]!);
  });

  test('slash "/" is supported as a keyboard shortcut', async () => {
    document.body.innerHTML = '<find-shortcut-element></find-shortcut-element>';
    const testElement = document.body.querySelector<FindShortcutElement>(
        'find-shortcut-element')!;
    testElement.hasFocus = false;
    await check(testElement, false, pressSlash);
  });
});
