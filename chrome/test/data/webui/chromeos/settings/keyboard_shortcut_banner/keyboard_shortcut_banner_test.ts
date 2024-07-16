// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {KeyboardShortcutBanner, sanitizeInnerHtml} from 'chrome://os-settings/lazy_load.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {MetaKey, Modifier, ShortcutLabelProperties} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {AcceleratorKeyState} from 'chrome://resources/mojo/ui/base/accelerators/mojom/accelerator.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<keyboard-shortcut-banner>', () => {
  let banner: KeyboardShortcutBanner;

  const TITLE = 'Keyboard shortcut available';
  // A description with the <kbd> elements in the middle of the sentence.
  const DESCRIPTIONS = [
    'Press <kbd><kbd>Ctrl</kbd>+<kbd>Space</kbd></kbd> to switch to the ' +
        'last used input method',
    'Press <kbd><kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Space</kbd></kbd> to ' +
        'switch to the next input method',
  ].map((html) => sanitizeInnerHtml(html));

  /**
   * Setup the document with a single keyboard-shortcut-banner element with
   * the specified body. The `banner` variable is updated to be the new
   * element.
   */
  function setupBannerAndDocument(title: string, body: TrustedHTML[]): void {
    clearBody();

    banner = document.createElement('keyboard-shortcut-banner');
    banner.setAttribute('header', title);
    banner.setAttribute('body', JSON.stringify(body));
    banner.body = body;

    document.body.appendChild(banner);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({
      isShortcutCustomizationEnabled: false,
    });
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
    const altTabMessage = sanitizeInnerHtml(
        'Press and hold <kbd><kbd>Alt</kbd>+<kbd>Shift</kbd></kbd>, tap ' +
        '<kbd><kbd>Tab</kbd></kbd> until you get to the window you want to ' +
        'open, then release.');
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

  test('displays the shortcut-input-key', () => {
    banner.set('showCustomizedShortcut_', true);
    flush();
    const expectedAcceleratorProperties: ShortcutLabelProperties[] = [{
      keyDisplay: stringToMojoString16('m'),
      accelerator: {
        modifiers: Modifier.CONTROL,
        keyCode: VKey.kKeyM,
        keyState: AcceleratorKeyState.PRESSED,
        timeStamp: {
          internalValue: 0n,
        },
      },
      originalAccelerator: null,
      shortcutLabelText: getTrustedHTML`<a>test string</a>` as TrustedHTML,
      metaKey: MetaKey.kSearch,
    }];
    banner.shortcutLabelProperties = expectedAcceleratorProperties;

    // After setup showCustomizedShortcut and accelerator, parts-container
    // should show up to display shortcut-input-key.
    const shortcutInputKeyContainer =
        banner.shadowRoot!.querySelector('#partsContainer');
    const reminder = shortcutInputKeyContainer!.firstElementChild;

    const text = reminder!.firstElementChild;
    assertTrue(!!text);
    assertEquals(text.childNodes.length, 2);

    const [modifierNode, keyNode] = text.children;
    assertEquals((modifierNode as ShortcutInputKeyElement).key, 'ctrl');
    assertEquals((keyNode as ShortcutInputKeyElement).key, 'm');
  });
});
