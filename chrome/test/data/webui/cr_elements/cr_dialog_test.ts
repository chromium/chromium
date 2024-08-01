// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {keyDownOn, keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {assertEquals, assertFalse, assertNotEquals, assertNotReached, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-dialog', function() {
  function pressEnter(element: HTMLElement) {
    keyEventOn(element, 'keypress', 13, [], 'Enter');
  }

  /**
   * Creates and shows two nested cr-dialogs.
   * @return An array of 2 dialogs. The first dialog
   *     is the outer dialog, and the second is the inner dialog.
   */
  function createAndShowNestedDialogs(): [CrDialogElement, CrDialogElement] {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog id="outer">
        <div slot="title">outer dialog title</div>
        <div slot="body">
          <cr-dialog id="inner">
            <div slot="title">inner dialog title</div>
            <div slot="body">body</div>
          </cr-dialog>
        </div>
      </cr-dialog>`;

    const outer = document.body.querySelector<CrDialogElement>('#outer');
    assertTrue(!!outer);
    const inner = document.body.querySelector<CrDialogElement>('#inner');
    assertTrue(!!inner);

    outer!.showModal();
    inner!.showModal();
    return [outer!, inner!];
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Ensure svg, which is referred to by a relative URL, is loaded from
    // chrome://resources and not chrome://test
    const base = document.createElement('base');
    base.href = 'chrome://resources/cr_elements/';
    document.head.appendChild(base);
  });

  test('cr-dialog-open event fires when opened', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const whenFired = eventToPromise('cr-dialog-open', dialog);
    dialog.showModal();
    return whenFired;
  });

  test('close event bubbles', async function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();
    const whenFired = eventToPromise('close', dialog);
    dialog.close();
    await whenFired;
    assertEquals('success', dialog.getNative().returnValue);
  });

  // cr-dialog has to catch and re-fire 'close' events fired from it's native
  // <dialog> child to force them to bubble in Shadow DOM V1. Ensure that this
  // mechanism does not interfere with nested <cr-dialog> 'close' events.
  test(
      'close events not fired from <dialog> are not affected',
      async function() {
        const dialogs = createAndShowNestedDialogs();
        const outer = dialogs[0];
        const inner = dialogs[1];

        let whenFired = eventToPromise('close', window);
        inner.close();

        let e = await whenFired;
        // Check that the event's target is the inner dialog.
        assertEquals(inner, e.target);
        whenFired = eventToPromise('close', window);
        outer.close();
        e = await whenFired;
        // Check that the event's target is the outer dialog.
        assertEquals(outer, e.target);
      });

  test('cancel and close events bubbles when cancelled', async function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();
    const whenCancelFired = eventToPromise('cancel', dialog);
    const whenCloseFired = eventToPromise('close', dialog);
    dialog.cancel();
    await Promise.all([whenCancelFired, whenCloseFired]);
    assertEquals('', dialog.getNative().returnValue);
  });

  // cr-dialog has to catch and re-fire 'cancel' events fired from it's native
  // <dialog> child to force them to bubble in Shadow DOM V1. Ensure that this
  // mechanism does not interfere with nested <cr-dialog> 'cancel' events.
  test(
      'cancel events not fired from <dialog> are not affected',
      async function() {
        const dialogs = createAndShowNestedDialogs();
        const outer = dialogs[0];
        const inner = dialogs[1];

        let whenFired = eventToPromise('cancel', window);
        inner.cancel();

        let e = await whenFired;
        // Check that the event's target is the inner dialog.
        assertEquals(inner, e.target);
        whenFired = eventToPromise('cancel', window);
        outer.cancel();
        e = await whenFired;
        // Check that the event's target is the outer dialog.
        assertEquals(outer, e.target);
      });

  test('focuses title on show', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body"><button>button</button></div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    assertEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);
  });

  test('enter keys should trigger action buttons once', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog show-close-button>
        <div slot="title">title</div>
        <div slot="body">
          <button class="action-button">button</button>
          <button id="other-button">other button</button>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const actionButton =
        document.body.querySelector<HTMLElement>('.action-button')!;

    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    let clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });

    function simulateEnterOnButton(button: HTMLElement) {
      pressEnter(button);
      // Also call manually click() since normally this is done by the browser.
      button.click();
    }

    // Enter key on the action button should only fire the click handler once.
    simulateEnterOnButton(actionButton);
    assertEquals(1, clickedCounter);

    // Enter keys on other buttons should be ignored.
    clickedCounter = 0;
    const otherButton =
        document.body.querySelector<HTMLElement>('#other-button');
    assertTrue(!!otherButton);
    simulateEnterOnButton(otherButton!);
    assertEquals(0, clickedCounter);

    // Enter keys on the close icon in the top-right corner should be ignored.
    const close = dialog.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!close);
    pressEnter(close);
    assertEquals(0, clickedCounter);
  });

  test('enter keys find the first non-hidden non-disabled button', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <button id="hidden" class="action-button" hidden>hidden</button>
          <button class="action-button" disabled>disabled</button>
          <button class="action-button" disabled hidden>disabled hidden</button>
          <button id="active" class="action-button">active</button>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const hiddenButton = document.body.querySelector<HTMLElement>('#hidden')!;
    const actionButton = document.body.querySelector<HTMLElement>('#active')!;
    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    hiddenButton.addEventListener('click', function() {
      assertNotReached('Hidden button received a click.');
    });
    let clicked = false;
    actionButton.addEventListener('click', function() {
      clicked = true;
    });

    pressEnter(dialog);
    assertTrue(clicked);
  });

  test('enter keys from certain inputs only are processed', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <foobar></foobar>
          <input type="checkbox">
          <input type="text">

          <cr-input type="search"></cr-input>
          <cr-input type="text"></cr-input>

          <div id="withShadow"></div>
          <button class="action-button">active</button>
        </div>
      </cr-dialog>`;

    const otherElement = document.body.querySelector<HTMLElement>('foobar')!;
    const inputCheckboxElement =
        document.body.querySelector<HTMLElement>('input[type="checkbox"]')!;
    const inputTextElement =
        document.body.querySelector<HTMLElement>('input[type="text"]')!;

    const crTextInputElement =
        document.body.querySelector<CrInputElement>('cr-input[type="text"]')!;
    const crSearchInputElement =
        document.body.querySelector<CrInputElement>('cr-input[type="search"]')!;

    // Attach a cr-input element nested within another element.
    const containerElement = document.body.querySelector('#withShadow')!;
    const shadow = containerElement.attachShadow({mode: 'open'});
    const crInputNested = document.createElement('cr-input');
    shadow.appendChild(crInputNested);

    const actionButton = document.body.querySelector('.action-button')!;

    // MockInteractions triggers event listeners synchronously.
    let clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });

    // Enter on anything other than cr-input should not be accepted.
    pressEnter(otherElement);
    assertEquals(0, clickedCounter);
    pressEnter(inputCheckboxElement);
    assertEquals(0, clickedCounter);
    pressEnter(inputTextElement);
    assertEquals(0, clickedCounter);

    // Enter on a cr-input with type "search" should not be accepted.
    pressEnter(crSearchInputElement);
    assertEquals(0, clickedCounter);

    // Enter on a cr-input with type "text" should be accepted.
    pressEnter(crTextInputElement);
    assertEquals(1, clickedCounter);

    // Enter on a nested <cr-input> should be accepted.
    pressEnter(crInputNested);
    assertEquals(2, clickedCounter);
  });

  test('focuses [autofocus] instead of title when present', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body"><button autofocus>button</button></div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    assertNotEquals(dialog, document.activeElement);
    assertEquals(button, document.activeElement);
  });

  // Test that a cr-input[autofocus] is picked up by the browser when residing
  // within a <cr-dialog show-on-attach> which itself resides in a conditional
  // Lit template. Regression test for crbug/341327469.
  test('FocusesCrLitElementsWithAutofocus', async function() {
    class TestElement extends CrLitElement {
      static get is() {
        return 'test-element';
      }

      override render() {
        // clang-format off
        return html`
          ${this.showDialog ? html`
            <cr-dialog show-on-attach>
              <div slot="title">title</div>
              <div slot="body">
                <cr-input ?autofocus="${this.autofocusCrInput}"></cr-input>
                <cr-textarea ?autofocus="${this.autofocusCrTextarea}">
                </cr-textarea>
              </div>
            </cr-dialog>` : ''}`;
        // clang-format on
      }

      static override get properties() {
        return {
          showDialog: {type: Boolean},
          autofocusCrInput: {type: Boolean},
          autofocusCrTextarea: {type: Boolean},
        };
      }

      showDialog: boolean = false;
      autofocusCrInput: boolean = false;
      autofocusCrTextarea: boolean = false;
    }

    customElements.define(TestElement.is, TestElement);

    async function assertAutofocus(useTextarea: boolean) {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const element = document.createElement('test-element') as TestElement;
      useTextarea ? element.autofocusCrTextarea = true :
                    element.autofocusCrInput = true;
      const whenOpen = eventToPromise('cr-dialog-open', document.body);
      document.body.appendChild(element);
      element.showDialog = true;
      await whenOpen;

      const child = element.shadowRoot!.querySelector(
          useTextarea ? 'cr-textarea' : 'cr-input')!;
      assertEquals(
          useTextarea ? (child as CrTextareaElement).$.input :
                        (child as CrInputElement).inputElement,
          getDeepActiveElement());
    }

    await assertAutofocus(/*useTextarea=*/ false);
    await assertAutofocus(/*useTextarea=*/ true);
  });

  // Ensuring that intersectionObserver does not fire any callbacks before the
  // dialog has been opened.
  test('body scrollable border not added before modal shown', async function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    assertFalse(dialog.open);
    const bodyContainer = dialog.shadowRoot!.querySelector('.body-container');
    assertTrue(!!bodyContainer);
    const topShadow =
        dialog.shadowRoot!.querySelector('#cr-container-shadow-top');
    assertTrue(!!topShadow);
    const bottomShadow =
        dialog.shadowRoot!.querySelector('#cr-container-shadow-bottom');
    assertTrue(!!bottomShadow);

    await microtasksFinished();
    assertFalse(topShadow!.classList.contains('has-shadow'));
    assertFalse(bottomShadow!.classList.contains('has-shadow'));
  });

  test('dialog body scrollable border when appropriate', function(done) {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <div style="height: 100px">tall content</div>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    const bodyContainer =
        dialog.shadowRoot!.querySelector<HTMLElement>('.body-container');
    assertTrue(!!bodyContainer);
    const topShadow = dialog.shadowRoot!.querySelector<HTMLElement>(
        '#cr-container-shadow-top');
    assertTrue(!!topShadow);
    const bottomShadow = dialog.shadowRoot!.querySelector<HTMLElement>(
        '#cr-container-shadow-bottom');
    assertTrue(!!bottomShadow);

    dialog.showModal();  // Attach the dialog for the first time here.

    let observerCount = 0;

    function hasTransparentBorder(element: HTMLElement): boolean {
      const style = element.computedStyleMap().get('border-bottom-color') as
          CSSStyleValue;
      return style.toString() === 'rgba(0, 0, 0, 0)';
    }

    // Needs to setup the observer before attaching, since InteractionObserver
    // calls callback before MutationObserver does.
    const observer = new MutationObserver(function(changes) {
      // Only care about class mutations.
      if (changes[0]!.attributeName !== 'class') {
        return;
      }

      observerCount++;
      switch (observerCount) {
        case 1:  // Triggered when scrolled to bottom.
          assertTrue(hasTransparentBorder(bottomShadow!));
          assertFalse(hasTransparentBorder(topShadow!));
          bodyContainer!.scrollTop = 0;
          break;
        case 2:  // Triggered when scrolled back to top.
          assertFalse(hasTransparentBorder(bottomShadow));
          assertTrue(hasTransparentBorder(topShadow));
          bodyContainer!.scrollTop = 2;
          break;
        case 3:  // Triggered when finally scrolling to middle.
          assertFalse(hasTransparentBorder(bottomShadow!));
          assertFalse(hasTransparentBorder(topShadow!));
          observer.disconnect();
          done();
          break;
      }
    });
    observer.observe(bodyContainer!, {attributes: true});

    // Height is normally set via CSS, but mixin doesn't work with innerHTML.
    bodyContainer!.style.height = '60px';  // Element has "min-height: 60px".
    bodyContainer!.scrollTop = 100;
  });

  test(
      'dialog `open` attribute updated when Escape is pressed',
      async function() {
        document.body.innerHTML = getTrustedHTML`
          <cr-dialog>
            <div slot="title">title</div>
          </cr-dialog>`;

        const dialog = document.body.querySelector('cr-dialog')!;

        const whenOpen = eventToPromise('cr-dialog-open', dialog);
        dialog.showModal();
        await whenOpen;
        assertTrue(dialog.open);
        assertTrue(dialog.hasAttribute('open'));

        const whenCancel = eventToPromise('cancel', dialog);
        const e = new CustomEvent('cancel', {cancelable: true});
        dialog.getNative().dispatchEvent(e);
        await whenCancel;
        assertFalse(dialog.open);
        assertFalse(dialog.hasAttribute('open'));
      });

  test('dialog cannot be cancelled when `no-cancel` is set', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog no-cancel>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    assertTrue(dialog.noCancel);
    dialog.showModal();

    assertNull(dialog.shadowRoot!.querySelector('#close'));

    // Hitting escape fires a 'cancel' event. Cancelling that event prevents the
    // dialog from closing.
    let e = new CustomEvent('cancel', {cancelable: true});
    dialog.getNative().dispatchEvent(e);
    assertTrue(e.defaultPrevented);

    dialog.noCancel = false;

    e = new CustomEvent('cancel', {cancelable: true});
    dialog.getNative().dispatchEvent(e);
    assertFalse(e.defaultPrevented);
  });

  test('dialog close button shown when showCloseButton is true', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog show-close-button>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    assertTrue(dialog.showCloseButton);
    dialog.showModal();
    assertTrue(dialog.open);

    const close = dialog.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!close);
    assertTrue(isVisible(close));
    close.click();
    assertFalse(dialog.open);
  });

  test('dialog close button hidden when showCloseButton is false', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();

    assertNull(dialog.shadowRoot!.querySelector('#close'));
  });

  test(
      'keydown should be consumed when the property is true', async function() {
        document.body.innerHTML = getTrustedHTML`
      <cr-dialog consume-keydown-event>
        <div slot="title">title</div>
      </cr-dialog>`;

        const dialog = document.body.querySelector('cr-dialog')!;
        dialog.showModal();
        assertTrue(dialog.open);
        assertTrue(dialog.consumeKeydownEvent);

        function assertKeydownNotReached() {
          assertNotReached('keydown event was propagated');
        }
        document.addEventListener('keydown', assertKeydownNotReached);

        await microtasksFinished();
        keyDownOn(dialog, 65, [], 'a');
        keyDownOn(document.body, 65, [], 'a');
        document.removeEventListener('keydown', assertKeydownNotReached);
      });

  test(
      'keydown should be propagated when the property is false',
      async function() {
        document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;

        const dialog = document.body.querySelector('cr-dialog')!;
        dialog.showModal();
        assertTrue(dialog.open);
        assertFalse(dialog.consumeKeydownEvent);

        let keydownCounter = 0;
        function assertKeydownCount() {
          keydownCounter++;
        }
        document.addEventListener('keydown', assertKeydownCount);

        await microtasksFinished();
        keyDownOn(dialog, 65, [], 'a');
        assertEquals(1, keydownCounter);
        document.removeEventListener('keydown', assertKeydownCount);
      });

  test('show-on-attach', () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog show-on-attach>
        <div slot="title">title</div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog')!;
    assertTrue(dialog.showOnAttach);
    assertTrue(dialog.open);
  });

  test('close-text', async () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog close-text="foo" show-close-button>
        <div slot="title">title</div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();

    assertEquals('foo', dialog.closeText);
    const close = dialog.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!close);
    assertEquals('foo', close.ariaLabel);
    assertEquals('foo', close.getAttribute('aria-label'));

    dialog.closeText = undefined;
    await dialog.updateComplete;
    assertEquals(null, close.ariaLabel);
    assertFalse(close.hasAttribute('aria-label'));
  });

  // Test that when ignoreEnterKey is set, pressing "Enter" does not trigger the
  // action button.
  test('ignore-enter-key', () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog ignore-enter-key>
        <div slot="title">title</div>
        <div slot="body">
          <button class="action-button">button</button>
        </div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog')!;
    dialog.showModal();

    assertTrue(dialog.ignoreEnterKey);

    // MockInteractions triggers event listeners synchronously.
    const actionButton =
        document.body.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);

    let clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });
    pressEnter(dialog);

    assertEquals(0, clickedCounter);
  });

  test('close on popstate', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog')!;
    assertFalse(dialog.ignorePopstate);
    dialog.showModal();
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertFalse(dialog.open);
  });

  test('ignore-pop-state', () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-dialog ignore-popstate>
        <div slot="title">title</div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog')!;
    assertTrue(dialog.ignorePopstate);

    dialog.showModal();
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertTrue(dialog.open);
  });
});
