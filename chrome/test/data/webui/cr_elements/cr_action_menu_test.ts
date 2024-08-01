// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import type { CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {html, css, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';
import {getTrustedHTML as getTrustedStaticHtml} from 'chrome://resources/js/static_types.js';
// clang-format on

/**
 * @fileoverview Tests for cr-action-menu element. Runs as an interactive UI
 * test, since many of these tests check focus behavior.
 */
suite('CrActionMenu', function() {
  let menu: CrActionMenuElement;
  let dialog: HTMLDialogElement;
  let items: NodeListOf<HTMLElement>;
  let dots: HTMLElement;
  let container: HTMLElement;
  let checkboxFocusableElement: Element|null = null;

  setup(function() {
    FocusOutlineManager.forDocument(document).visible = false;
    document.body.innerHTML = getTrustedStaticHtml`
      <button id="dots">...</button>
      <cr-action-menu>
        <button class="dropdown-item">Un</button>
        <hr>
        <button class="dropdown-item">Dos</button>
        <cr-checkbox class="dropdown-item">Tres</cr-checkbox>
      </cr-action-menu>
    `;

    menu = document.querySelector('cr-action-menu')!;
    dialog = menu.getDialog();
    items = menu.querySelectorAll('.dropdown-item');
    checkboxFocusableElement =
        (items[2] as CrCheckboxElement).getFocusableElement();
    dots = document.querySelector('#dots')!;
    assertEquals(3, items.length);
  });

  teardown(function() {
    document.body.style.direction = 'ltr';

    if (dialog.open) {
      menu.close();
    }
  });

  function down() {
    keyDownOn(menu, 0, [], 'ArrowDown');
  }

  function up() {
    keyDownOn(menu, 0, [], 'ArrowUp');
  }

  function enter() {
    keyDownOn(menu, 0, [], 'Enter');
  }

  test('open-changed event fires', async function() {
    let whenFired = eventToPromise('open-changed', menu);
    menu.showAt(dots);
    let event = await whenFired;
    assertTrue(event.detail.value);

    whenFired = eventToPromise('open-changed', menu);
    menu.close();
    event = await whenFired;
    assertFalse(event.detail.value);
  });

  test('close event bubbles', function() {
    menu.showAt(dots);
    const whenFired = eventToPromise('close', menu);
    menu.close();
    return whenFired;
  });

  test('hidden or disabled items', function() {
    menu.showAt(dots);
    down();
    assertEquals(items[0], getDeepActiveElement());

    menu.close();
    items[0]!.hidden = true;
    menu.showAt(dots);
    down();
    assertEquals(items[1], getDeepActiveElement());

    menu.close();
    (items[1] as HTMLButtonElement).disabled = true;
    menu.showAt(dots);
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('focus after down/up arrow', function() {
    menu.showAt(dots);

    // The menu should be focused when shown, but not on any of the items.
    assertEquals(menu, document.activeElement);
    assertNotEquals(items[0], getDeepActiveElement());
    assertNotEquals(items[1], getDeepActiveElement());
    assertNotEquals(checkboxFocusableElement, getDeepActiveElement());

    down();
    assertEquals(items[0], getDeepActiveElement());
    down();
    assertEquals(items[1], getDeepActiveElement());
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
    down();
    assertEquals(items[0], getDeepActiveElement());
    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
    up();
    assertEquals(items[1], getDeepActiveElement());
    up();
    assertEquals(items[0], getDeepActiveElement());
    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());

    (items[1] as HTMLButtonElement).disabled = true;
    up();
    assertEquals(items[0], getDeepActiveElement());
  });

  test('focus skips cr-checkbox when disabled or hidden', async () => {
    menu.showAt(dots);
    const crCheckbox = document.querySelector('cr-checkbox')!;
    assertEquals(items[2], crCheckbox);

    // Check checkbox is focusable when not disabled or hidden.
    down();
    assertEquals(items[0], getDeepActiveElement());
    down();
    assertEquals(items[1], getDeepActiveElement());
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());

    // Check checkbox is not focusable when either disabled or hidden.
    const cases: Array<[boolean, boolean]> = [
      [false, true],
      [true, false],
      [true, true],
    ];

    for (const [disabled, hidden] of cases) {
      crCheckbox.disabled = disabled;
      crCheckbox.hidden = hidden;
      await crCheckbox.updateComplete;
      (getDeepActiveElement() as HTMLElement).blur();
      down();
      assertEquals(items[0], getDeepActiveElement());
      down();
      assertEquals(items[1], getDeepActiveElement());
      down();
      assertEquals(items[0], getDeepActiveElement());
    }
  });

  test('pressing up arrow when no focus will focus last item', function() {
    menu.showAt(dots);
    assertEquals(menu, document.activeElement);

    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('pressing enter when no focus', function() {
    if (isWindows || isMac) {
      return testFocusAfterClosing('Enter');
    }

    // First item is selected
    menu.showAt(dots);
    assertEquals(menu, document.activeElement);
    enter();
    assertEquals(items[0], getDeepActiveElement());
    return;
  });

  test('pressing enter when when item has focus', function() {
    menu.showAt(dots);
    down();
    enter();
    assertEquals(items[0], getDeepActiveElement());
  });

  test('can navigate to dynamically added items', async function() {
    // Can modify children after attached() and before showAt().
    const item = document.createElement('button');
    item.classList.add('dropdown-item');
    menu.insertBefore(item, items[0]!);
    menu.showAt(dots);
    await microtasksFinished();

    down();
    assertEquals(item, getDeepActiveElement());
    down();
    assertEquals(items[0], getDeepActiveElement());

    // Can modify children while menu is open.
    menu.removeChild(item);

    up();
    // Focus should have wrapped around to final item.
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('close on click away', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);
    menu.click();
    assertFalse(dialog.open);
  });

  test('close on resize', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(dialog.open);
  });

  test('close on popstate', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertFalse(dialog.open);
  });

  /** @param key The key to use for closing. */
  function testFocusAfterClosing(key: string): Promise<void> {
    return new Promise<void>(function(resolve) {
      menu.showAt(dots);
      assertTrue(dialog.open);

      let anchorHasFocus = false;
      let tabkeyCloseEventFired = false;

      const checkTestDone = () => {
        assertFalse(dialog.open);
        if (key !== 'Tab') {
          resolve();
        } else if (anchorHasFocus && tabkeyCloseEventFired) {
          resolve();
        }
      };

      // Check that focus returns to the anchor element.
      dots.addEventListener('focus', () => {
        anchorHasFocus = true;
        checkTestDone();
      });

      // Check that a Tab key close fires a custom event.
      menu.addEventListener('tabkeyclose', () => {
        tabkeyCloseEventFired = true;
        checkTestDone();
      });

      keyDownOn(menu, 0, [], key);
    });
  }

  test('close on Tab', () => testFocusAfterClosing('Tab'));

  test('close on Escape', () => testFocusAfterClosing('Escape'));

  function dispatchMouseoverEvent(eventTarget: EventTarget) {
    eventTarget.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));
  }

  test('moving mouse on option 1 should focus it', () => {
    menu.showAt(dots);
    assertNotEquals(items[0], getDeepActiveElement());
    dispatchMouseoverEvent(items[0]!);
    assertEquals(items[0], getDeepActiveElement());
  });

  test('moving mouse on the menu (not on option) should focus the menu', () => {
    menu.showAt(dots);
    items[0]!.focus();
    dispatchMouseoverEvent(menu);
    assertEquals(dialog.querySelector('[role="menu"]'), getDeepActiveElement());
  });

  test('moving mouse on a disabled item should focus the menu', () => {
    menu.showAt(dots);
    items[2]!.toggleAttribute('disabled', true);
    items[0]!.focus();
    dispatchMouseoverEvent(items[2]!);
    assertEquals(dialog.querySelector('[role="menu"]'), getDeepActiveElement());
  });

  test('mouse movements should override keyboard focus', () => {
    menu.showAt(dots);
    items[0]!.focus();
    down();
    assertEquals(items[1], getDeepActiveElement());
    dispatchMouseoverEvent(items[0]!);
    assertEquals(items[0], getDeepActiveElement());
  });

  test('items automatically given accessibility role', async function() {
    const newItem = document.createElement('button');
    newItem.classList.add('dropdown-item');

    items[1]!.setAttribute('role', 'checkbox');
    menu.showAt(dots);

    await microtasksFinished();
    assertEquals('menuitem', items[0]!.getAttribute('role'));
    assertEquals('checkbox', items[1]!.getAttribute('role'));

    menu.insertBefore(newItem, items[0]!);
    await microtasksFinished();
    assertEquals('menuitem', newItem.getAttribute('role'));
  });

  test('positioning', function() {
    // A 40x10 box at (200, 250).
    const config = {
      left: 200,
      top: 250,
      width: 40,
      height: 10,
      maxX: 1000,
      maxY: 2000,
    };

    // By default, aligns top-left of menu with top-left of anchor.
    menu.showAtPosition(config);
    assertTrue(dialog.open);
    assertEquals(`${config.left}px`, dialog.style.left);
    assertEquals(`${config.top}px`, dialog.style.top);
    menu.close();

    // Align the menu's bottom-right to the anchor's top-left.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_START,
      anchorAlignmentY: AnchorAlignment.BEFORE_START,
    }));
    const menuHeight = dialog.offsetHeight;
    const menuWidth = dialog.offsetWidth;
    assertEquals(`${config.top - menuHeight}px`, dialog.style.top);
    assertEquals(`${config.left - menuWidth}px`, dialog.style.left);

    // Center the menu horizontally.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.CENTER,
    }));
    assertEquals(
        `${(config.left + config.width / 2) - menuWidth / 2}px`,
        dialog.style.left);
    assertEquals(`${config.top}px`, dialog.style.top);
    menu.close();

    // Center the menu in both axes.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.CENTER,
    }));
    assertEquals(
        `${(config.left + config.width / 2) - menuWidth / 2}px`,
        dialog.style.left);
    assertEquals(
        `${(config.top + config.height / 2) - menuHeight / 2}px`,
        dialog.style.top);
    menu.close();

    // Align bottom-right of menu to top-left of anchor.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_END,
    }));
    assertEquals(
        `${config.left + config.width - menuWidth}px`, dialog.style.left);
    assertEquals(
        `${config.top + config.height - menuHeight}px`, dialog.style.top);
    menu.close();

    // Being left and top aligned at (0, 0) should anchor to the bottom right.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_END,
      left: 0,
      top: 0,
    }));
    assertEquals(`0px`, dialog.style.left);
    assertEquals(`0px`, dialog.style.top);
    menu.close();

    // Being aligned to a point in the bottom right should anchor to the top
    // left.
    menu.showAtPosition({
      left: 1000,
      top: 2000,
      maxX: 1000,
      maxY: 2000,
    });
    assertEquals(`${1000 - menuWidth}px`, dialog.style.left);
    assertEquals(`${2000 - menuHeight}px`, dialog.style.top);
    menu.close();

    // If the viewport can't fit the menu, align the menu to the viewport.
    menu.showAtPosition({
      left: menuWidth - 5,
      top: 0,
      width: 0,
      height: 0,
      maxX: menuWidth * 2 - 10,
    });
    assertEquals(`${menuWidth - 10}px`, dialog.style.left);
    assertEquals(`0px`, dialog.style.top);
    menu.close();

    // Alignment is reversed in RTL.
    document.body.style.direction = 'rtl';
    menu.showAtPosition(config);
    assertTrue(dialog.open);
    assertEquals(config.left + config.width - menuWidth, dialog.offsetLeft);
    assertEquals(`${config.top}px`, dialog.style.top);
    menu.close();
  });

  function autoRepositionTest(done: () => void) {
    menu.autoReposition = true;

    dots.style.marginLeft = '800px';

    const dotsRect = dots.getBoundingClientRect();

    // Anchored at right-top by default.
    menu.showAt(dots);
    assertTrue(dialog.open);
    let menuRect = dialog.getBoundingClientRect();
    assertEquals(
        Math.round(dotsRect.left + dotsRect.width),
        Math.round(menuRect.left + menuRect.width));
    assertEquals(dotsRect.top, menuRect.top);

    const lastMenuLeft = menuRect.left;
    const lastMenuWidth = menuRect.width;

    menu.addEventListener('cr-action-menu-repositioned', () => {
      assertTrue(dialog.open);
      menuRect = dialog.getBoundingClientRect();
      // Test that menu width got larger.
      assertTrue(menuRect.width > lastMenuWidth);
      // Test that menu upper-left moved further left.
      assertTrue(menuRect.left < lastMenuLeft);
      // Test that right and top did not move since it is anchored there.
      assertEquals(
          Math.round(dotsRect.left + dotsRect.width),
          Math.round(menuRect.left + menuRect.width));
      assertEquals(dotsRect.top, menuRect.top);
      done();
    });

    // Still anchored at the right place after content size changes.
    items[0]!.textContent = 'this is a long string to make menu wide';
  }

  // <if expr="is_win or is_macosx">
  // TODO(dpapad): Figure out why it fails on windows only and re-enable.
  // TODO(crbug.com/329266310): Flakes on MacOS.
  test.skip(
      '[auto-reposition] enables repositioning if content changes',
      autoRepositionTest);
  // </if>
  // <if expr="not is_win and not is_macosx">
  test(
      '[auto-reposition] enables repositioning if content changes',
      autoRepositionTest);
  // </if>

  test('accessibilityLabel', async function() {
    document.body.innerHTML = getTrustedStaticHtml`
      <cr-action-menu accessibility-label="foo">
        <button class="dropdown-item">Un</button>
      </cr-action-menu>`;
    menu = document.querySelector('cr-action-menu')!;

    // Check initial state, populated from HTML markup.
    assertEquals('foo', menu.accessibilityLabel);
    assertEquals('foo', menu.$.wrapper.getAttribute('aria-label'));

    // Check value provided with direct assignment.
    const label: string = 'dummy label';
    menu.accessibilityLabel = label;
    await menu.updateComplete;
    assertEquals(label, menu.$.wrapper.ariaLabel);
    assertEquals(label, menu.$.wrapper.getAttribute('aria-label'));

    // Check setting to undefined.
    menu.accessibilityLabel = undefined;
    await menu.updateComplete;
    assertEquals(null, menu.$.wrapper.ariaLabel);
    assertFalse(menu.$.wrapper.hasAttribute('aria-label'));
  });

  test('roleDescription', async function() {
    document.body.innerHTML = getTrustedStaticHtml`
      <cr-action-menu role-description="foo">
        <button class="dropdown-item">Un</button>
      </cr-action-menu>`;
    menu = document.querySelector('cr-action-menu')!;

    // Check initial state, populated from HTML markup.
    assertEquals('foo', menu.roleDescription);
    assertEquals('foo', menu.$.dialog.ariaRoleDescription);
    assertEquals('foo', menu.$.dialog.getAttribute('aria-roledescription'));

    // Check value provided with direct assignment.
    const description: string = 'dummy description';
    menu.roleDescription = description;
    await menu.updateComplete;
    assertEquals(description, menu.$.dialog.ariaRoleDescription);
    assertEquals(
        description, menu.$.dialog.getAttribute('aria-roledescription'));

    // Check setting to undefined.
    menu.roleDescription = undefined;
    await menu.updateComplete;
    assertEquals(null, menu.$.dialog.ariaRoleDescription);
    assertFalse(menu.$.dialog.hasAttribute('aria-roledescription'));
  });

  suite('offscreen scroll positioning', function() {
    const bodyHeight = 10000;
    const bodyWidth = 20000;
    const containerLeft = 5000;
    const containerTop = 10000;
    const containerWidth = 500;

    class TestElement extends CrLitElement {
      static get is() {
        return 'test-element';
      }

      static override get styles() {
        return css`
          #container {
            overflow: auto;
            position: absolute;
            top: 10000px; /* containerTop */
            left: 5000px; /* containerLeft */
            right: 5000px; /* containerLeft */
            height: 500px; /* containerWidth */
            width: 500px; /* containerWidth */
          }

          #inner-container {
            height: 1000px;
            width: 1000px;
          }
        `;
      }

      override render() {
        return html`
          <div id="container">
            <div id="inner-container">
              <button id="dots">...</button>
              <cr-action-menu>
                <button class="dropdown-item">Un</button>
                <hr>
                <button class="dropdown-item">Dos</button>
                <button class="dropdown-item">Tres</button>
              </cr-action-menu>
            </div>
          </div>
        `;
      }
    }

    customElements.define(TestElement.is, TestElement);

    setup(function() {
      document.body.scrollTop = 0;
      document.body.scrollLeft = 0;
      document.body.innerHTML = getTrustedHtml(`
        <style>
          test-element {
            height: ${bodyHeight}px;
            width: ${bodyWidth}px;
          }
        </style>
        <test-element></test-element>`);

      const testElement = document.querySelector('test-element')!;
      menu = testElement.shadowRoot!.querySelector('cr-action-menu')!;
      dialog = menu.getDialog();
      dots = testElement.shadowRoot!.querySelector('#dots')!;
      container = testElement.shadowRoot!.querySelector('#container')!;
    });

    // Show the menu, scrolling the body to the button.
    test('simple offscreen', function() {
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, dialog.style.left);
      assertEquals(`${containerTop}px`, dialog.style.top);
      menu.close();
    });

    // Show the menu, scrolling the container to the button, and the body to the
    // button.
    test('offscreen and out of scroll container viewport', function() {
      document.body.scrollLeft = bodyWidth;
      document.body.scrollTop = bodyHeight;

      container.scrollLeft = containerLeft;
      container.scrollTop = containerTop;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, dialog.style.left);
      assertEquals(`${containerTop}px`, dialog.style.top);
      menu.close();
    });

    // Show the menu for an already onscreen button. The anchor should be
    // overridden so that no scrolling happens.
    test('onscreen forces anchor change', function() {
      const rect = dots.getBoundingClientRect();
      document.documentElement.scrollLeft =
          rect.right - document.documentElement.clientWidth + 10;
      document.documentElement.scrollTop =
          rect.bottom - document.documentElement.clientHeight + 10;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      const buttonWidth = dots.offsetWidth;
      const buttonHeight = dots.offsetHeight;
      const menuWidth = dialog.offsetWidth;
      const menuHeight = dialog.offsetHeight;
      assertEquals(containerLeft - menuWidth + buttonWidth, dialog.offsetLeft);
      assertEquals(containerTop - menuHeight + buttonHeight, dialog.offsetTop);
      menu.close();
    });

    test('scroll position maintained for showAtPosition', function() {
      document.documentElement.scrollLeft = 500;
      document.documentElement.scrollTop = 1000;
      menu.showAtPosition({top: 50, left: 50});
      assertEquals(550, dialog.offsetLeft);
      assertEquals(1050, dialog.offsetTop);
      menu.close();
    });

    test('rtl', function() {
      // Anchor to an item in RTL.
      document.body.style.direction = 'rtl';
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      const menuWidth = dialog.offsetWidth;
      assertEquals(
          container.offsetLeft + containerWidth - menuWidth, dialog.offsetLeft);
      assertEquals(containerTop, dialog.offsetTop);
      menu.close();
    });

    test('FocusFirstItemWhenOpenedWithKeyboard', async () => {
      FocusOutlineManager.forDocument(document).visible = true;
      menu.showAtPosition({top: 50, left: 50});
      await new Promise(resolve => requestAnimationFrame(resolve));
      assertEquals(
          menu.querySelector('.dropdown-item'), getDeepActiveElement());
    });
  });
});
