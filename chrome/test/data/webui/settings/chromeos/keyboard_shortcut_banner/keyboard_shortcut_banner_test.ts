// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {KeyboardShortcutBanner} from 'chrome://os-settings/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<keyboard-shortcut-banner>', () => {
  let banner: KeyboardShortcutBanner;

  const TITLE = 'Keyboard shortcut available';
  // A description with the <kbd> elements in the middle of the sentence.
  const DESCRIPTIONS = [
    'Press <kbd><kbd>Ctrl</kbd>+<kbd>Space</kbd></kbd> to switch to the ' +
        'last used input method',
    'Press <kbd><kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Space</kbd></kbd> to ' +
        'switch to the next input method',
  ];

  /**
   * Setup the document with a single keyboard-shortcut-banner element with
   * the specified body. The `banner` variable is updated to be the new
   * element.
   */
  function setupBannerAndDocument(title: string, body: string[]): void {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    banner = document.createElement('keyboard-shortcut-banner');
    banner.setAttribute('header', title);
    banner.setAttribute('body', JSON.stringify(body));

    document.body.appendChild(banner);
    flush();
  }

  setup(() => {
    setupBannerAndDocument(TITLE, DESCRIPTIONS);
  });

  /**
   * Tests that the descriptions have the expected <kbd> elements.
   * @return The number of child <kbd> elements in each top-level <kbd> element.
   */
  function testDescKbds(desc: Element): number[] {
    const allInnerKbds = [];

    for (const child of desc.childNodes) {
      if (child.nodeType === Node.TEXT_NODE) {
        continue;
      }
      assertEquals(Node.ELEMENT_NODE, child.nodeType);
      assertEquals('KBD', child.nodeName);
      let innerKbds = 0;
      // Each <kbd> should have nested <kbd>s, or a text node with just '+'.
      for (const kbdChild of child.childNodes) {
        if (kbdChild.nodeType === Node.TEXT_NODE) {
          assertEquals('+', kbdChild.textContent);
        } else {
          assertEquals(Node.ELEMENT_NODE, kbdChild.nodeType);
          assertEquals('KBD', kbdChild.nodeName);
          innerKbds++;

          // The nested <kbd>s' children should all be text nodes.
          assertTrue([...kbdChild.childNodes].every(
              child => child.nodeType === Node.TEXT_NODE));
        }
      }
      allInnerKbds.push(innerKbds);
    }

    return allInnerKbds;
  }

  test('displays the expected text', () => {
    const header = banner.shadowRoot!.querySelector('h2');
    assertEquals(TITLE, header!.textContent);
    const firstDesc = banner.shadowRoot!.querySelector('#id0');
    assertEquals(
        'Press Ctrl+Space to switch to the last used input method',
        firstDesc!.textContent);
    const secondDesc = banner.shadowRoot!.querySelector('#id1');
    assertEquals(
        'Press Ctrl+Shift+Space to switch to the next input method',
        secondDesc!.textContent);
    const dismissButton = banner.shadowRoot!.querySelector('cr-button');
    assertEquals('Dismiss', dismissButton!.textContent!.trim());
  });

  test('displays the correct <kbd> elements', () => {
    const firstDesc = banner.shadowRoot!.querySelector('#id0');
    const secondDesc = banner.shadowRoot!.querySelector('#id1');
    assertDeepEquals([2], testDescKbds(firstDesc!));
    assertDeepEquals([3], testDescKbds(secondDesc!));

    // Test multiple top-level <kbd> elements.
    const altTabMessage =
        'Press and hold <kbd><kbd>Alt</kbd>+<kbd>Shift</kbd></kbd>, tap ' +
        '<kbd><kbd>Tab</kbd></kbd> until you get to the window you want to ' +
        'open, then release.';
    setupBannerAndDocument(TITLE, [altTabMessage]);
    const altTabDesc = banner.shadowRoot!.querySelector('#id0');
    assertDeepEquals([2, 1], testDescKbds(altTabDesc!));
  });

  test('fires the dismiss event on button click', async () => {
    const dismissPromise = eventToPromise('dismiss', banner);
    const button = banner.shadowRoot!.querySelector('cr-button');
    button!.click();
    await dismissPromise;
  });
});
