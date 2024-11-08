// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';

import type {CrRippleElement} from 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrRipple', function() {
  let ripple: CrRippleElement;

  let setPointerCaptureCalls: number[];
  let releasePointerCaptureCalls: number[];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ripple = document.createElement('cr-ripple');
    document.body.appendChild(ripple);

    setPointerCaptureCalls = [];
    releasePointerCaptureCalls = [];

    // Overriding setPointerCapture, releasePointerCapture, to verify they are
    // called as expected during tests.
    ripple.setPointerCapture = function(id: number) {
      setPointerCaptureCalls.push(id);
    };

    ripple.releasePointerCapture = function(id: number) {
      releasePointerCaptureCalls.push(id);
    };

    ripple.hasPointerCapture = function(id: number): boolean {
      return setPointerCaptureCalls.includes(id);
    };
  });

  function pointerdown() {
    ripple.parentElement!.dispatchEvent(
        new PointerEvent('pointerdown', {pointerId: 1}));
  }

  function pointermove() {
    ripple.parentElement!.dispatchEvent(
        new PointerEvent('pointermove', {pointerId: 1}));
  }

  function pointerup() {
    ripple.parentElement!.dispatchEvent(
        new PointerEvent('pointerup', {pointerId: 1}));
  }

  function pointercancel() {
    ripple.parentElement!.dispatchEvent(
        new PointerEvent('pointercancel', {pointerId: 1}));
  }

  function keydown(key: string) {
    ripple.parentElement!.dispatchEvent(
        new KeyboardEvent('keydown', {key, cancelable: true}));
  }

  function keyup(key: string) {
    ripple.parentElement!.dispatchEvent(new KeyboardEvent('keyup', {key}));
  }

  function assertRipplesShown(count: number, triggerFn: () => void) {
    return new Promise<void>(resolve => {
      const addedNodes = [];
      const removedNodes = [];

      const observer = new MutationObserver((records: MutationRecord[]) => {
        for (const record of records) {
          if (record.type !== 'childList') {
            continue;
          }

          for (const node of [...record.addedNodes, ...record.removedNodes]) {
            assertEquals(Node.ELEMENT_NODE, node.nodeType);
            assertTrue((node as Element).classList.contains('ripple'));
          }

          addedNodes.push(...record.addedNodes);
          removedNodes.push(...record.removedNodes);

          if (removedNodes.length === count) {
            assertEquals(count, addedNodes.length);
            resolve();
          }
        }
      });
      observer.observe(ripple.shadowRoot!, {childList: true});
      triggerFn();
    });
  }

  function assertRipplesNotShown(triggerFn: () => void) {
    return new Promise<void>(resolve => {
      const observer = new MutationObserver((records: MutationRecord[]) => {
        for (const record of records) {
          if (record.type !== 'childList') {
            continue;
          }

          assertNotReached('Unexpected ripple shown');
        }
      });
      observer.observe(ripple.shadowRoot!, {childList: true});

      // Yield to ensure that any unexpected ripples have a chance to surface.
      window.setTimeout(() => {
        resolve();
      }, 1);

      triggerFn();
    });
  }

  test('DefaultState', function() {
    assertFalse(ripple.noink);
    assertFalse(ripple.holdDown);
    assertFalse(ripple.recenters);
  });

  test('RippleShown_EnterKey', function() {
    return assertRipplesShown(1, () => {
      keydown('Enter');
    });
  });

  test('RippleShown_SpaceKey', function() {
    return assertRipplesShown(1, () => {
      keydown(' ');
      keyup(' ');
    });
  });

  test('RippleShown_PointerDownUp', function() {
    return assertRipplesShown(1, () => {
      pointerdown();
      pointerup();

      assertArrayEquals([], setPointerCaptureCalls);
      assertArrayEquals([], releasePointerCaptureCalls);
    });
  });

  test('RippleShown_PointerDownMoveUp', function() {
    return assertRipplesShown(1, () => {
      pointerdown();
      pointermove();
      pointermove();
      pointerup();

      assertArrayEquals([1], setPointerCaptureCalls);
      assertArrayEquals([1], releasePointerCaptureCalls);
    });
  });

  test('RippleShown_PointerDownCancel', function() {
    return assertRipplesShown(1, () => {
      pointerdown();
      pointercancel();

      assertArrayEquals([], setPointerCaptureCalls);
      assertArrayEquals([], releasePointerCaptureCalls);
    });
  });

  test('RippleShown_PointerDownMoveCancel', function() {
    return assertRipplesShown(1, () => {
      pointerdown();
      pointermove();
      pointermove();
      pointercancel();

      assertArrayEquals([1], setPointerCaptureCalls);
      assertArrayEquals([1], releasePointerCaptureCalls);
    });
  });

  test('uiDownAction_Single', function() {
    return assertRipplesShown(1, () => {
      ripple.uiDownAction();
      ripple.uiUpAction();
    });
  });

  test('uiDownAction_Multiple', function() {
    return assertRipplesShown(3, () => {
      ripple.uiDownAction();
      ripple.uiDownAction();
      ripple.uiDownAction();
      ripple.uiUpAction();
      ripple.uiUpAction();
      ripple.uiUpAction();
    });
  });

  test('showAndHoldDown', function() {
    return assertRipplesShown(1, async () => {
      ripple.showAndHoldDown();
      await ripple.updateComplete;
      ripple.clear();
    });
  });

  test('noink', async function() {
    ripple.noink = true;
    await assertRipplesNotShown(() => {
      keydown('Enter');
    });

    await assertRipplesNotShown(() => {
      keydown(' ');
      keyup(' ');
    });

    await assertRipplesNotShown(() => {
      pointerdown();
      pointerup();
    });

    return assertRipplesNotShown(() => {
      ripple.uiDownAction();
      ripple.uiUpAction();
    });
  });

  test('RippleNotShown_EnterKeyDefaultPrevenetd', function() {
    ripple.parentElement!.addEventListener('keydown', e => {
      e.preventDefault();
    }, {once: true, capture: true});

    return assertRipplesNotShown(() => {
      keydown('Enter');
    });
  });
});
