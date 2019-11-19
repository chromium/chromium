// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
//
// #import {eventToPromise, flushTasks} from '../test_util.m.js';
// #import {keyDownOn, keyEventOn, tap} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('cr-dialog', function() {
  function pressEnter(element) {
    MockInteractions.keyEventOn(element, 'keypress', 13, undefined, 'Enter');
  }

  /**
   * Creates and shows two nested cr-dialogs.
   * @return {!Array<!CrDialogElement>} An array of 2 dialogs. The first dialog
   *     is the outer dialog, and the second is the inner dialog.
   */
  function createAndShowNestedDialogs() {
    document.body.innerHTML = `
      <cr-dialog id="outer">
        <div slot="title">outer dialog title</div>
        <div slot="body">
          <cr-dialog id="inner">
            <div slot="title">inner dialog title</div>
            <div slot="body">body</div>
          </cr-dialog>
        </div>
      </cr-dialog>`;

    const outer = document.body.querySelector('#outer');
    assertTrue(!!outer);
    const inner = document.body.querySelector('#inner');
    assertTrue(!!inner);

    outer.showModal();
    inner.showModal();
    return [outer, inner];
  }

  setup(function() {
    PolymerTest.clearBody();
    // Ensure svg, which is referred to by a relative URL, is loaded from
    // chrome://resources and not chrome://test
    const base = document.createElement('base');
    base.href = 'chrome://resources/cr_elements/';
    document.head.appendChild(base);
  });

  test('cr-dialog-open event fires when opened', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const whenFired = test_util.eventToPromise('cr-dialog-open', dialog);
    dialog.showModal();
    return whenFired;
  });

  test('close event bubbles', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    const whenFired = test_util.eventToPromise('close', dialog);
    dialog.close();
    return whenFired.then(() => {
      assertEquals('success', dialog.getNative().returnValue);
    });
  });

  // cr-dialog has to catch and re-fire 'close' events fired from it's native
  // <dialog> child to force them to bubble in Shadow DOM V1. Ensure that this
  // mechanism does not interfere with nested <cr-dialog> 'close' events.
  test('close events not fired from <dialog> are not affected', function() {
    const dialogs = createAndShowNestedDialogs();
    const outer = dialogs[0];
    const inner = dialogs[1];

    let whenFired = test_util.eventToPromise('close', window);
    inner.close();

    return whenFired
        .then(e => {
          // Check that the event's target is the inner dialog.
          assertEquals(inner, e.target);
          whenFired = test_util.eventToPromise('close', window);
          outer.close();
          return whenFired;
        })
        .then(e => {
          // Check that the event's target is the outer dialog.
          assertEquals(outer, e.target);
        });
  });

  test('cancel and close events bubbles when cancelled', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    const whenCancelFired = test_util.eventToPromise('cancel', dialog);
    const whenCloseFired = test_util.eventToPromise('close', dialog);
    dialog.cancel();
    return Promise.all([whenCancelFired, whenCloseFired]).then(() => {
      assertEquals('', dialog.getNative().returnValue);
    });
  });

  // cr-dialog has to catch and re-fire 'cancel' events fired from it's native
  // <dialog> child to force them to bubble in Shadow DOM V1. Ensure that this
  // mechanism does not interfere with nested <cr-dialog> 'cancel' events.
  test('cancel events not fired from <dialog> are not affected', function() {
    const dialogs = createAndShowNestedDialogs();
    const outer = dialogs[0];
    const inner = dialogs[1];

    let whenFired = test_util.eventToPromise('cancel', window);
    inner.cancel();

    return whenFired
        .then(e => {
          // Check that the event's target is the inner dialog.
          assertEquals(inner, e.target);
          whenFired = test_util.eventToPromise('cancel', window);
          outer.cancel();
          return whenFired;
        })
        .then(e => {
          // Check that the event's target is the outer dialog.
          assertEquals(outer, e.target);
        });
  });

  test('focuses title on show', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body"><button>button</button></div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectEquals(dialog, document.activeElement);
    expectNotEquals(button, document.activeElement);
  });

  test('enter keys should trigger action buttons once', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <button class="action-button">button</button>
          <button id="other-button">other button</button>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const actionButton = document.body.querySelector('.action-button');

    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    let clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });

    // Enter key on the action button should only fire the click handler once.
    MockInteractions.tap(actionButton, 'keypress', 13, undefined, 'Enter');
    assertEquals(1, clickedCounter);

    // Enter keys on other buttons should be ignored.
    clickedCounter = 0;
    const otherButton = document.body.querySelector('#other-button');
    assertTrue(!!otherButton);
    pressEnter(otherButton);
    assertEquals(0, clickedCounter);

    // Enter keys on the close icon in the top-right corner should be ignored.
    pressEnter(dialog.$.close);
    assertEquals(0, clickedCounter);
  });

  test('enter keys find the first non-hidden non-disabled button', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <button id="hidden" class="action-button" hidden>hidden</button>
          <button class="action-button" disabled>disabled</button>
          <button class="action-button" disabled hidden>disabled hidden</button>
          <button id="active" class="action-button">active</button>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const hiddenButton = document.body.querySelector('#hidden');
    const actionButton = document.body.querySelector('#active');
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
    document.body.innerHTML = `
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

    const dialog = document.body.querySelector('cr-dialog');

    const otherElement = document.body.querySelector('foobar');
    const inputCheckboxElement =
        document.body.querySelector('input[type="checkbox"]');
    const inputTextElement = document.body.querySelector('input[type="text"]');

    // Manually set the |type| property since cr-input is not actually imported
    // as part of this test, and therefore the element is not upgraded, as it
    // would normally.
    const crTextInputElement =
        document.body.querySelector('cr-input[type="text"]');
    crTextInputElement.type = 'text';
    const crSearchInputElement =
        document.body.querySelector('cr-input[type="search"]');
    crSearchInputElement.type = 'search';

    // Attach a cr-input element nested within another element.
    const containerElement = document.body.querySelector('#withShadow');
    const shadow = containerElement.attachShadow({mode: 'open'});
    const crInputNested = document.createElement('cr-input');
    crInputNested.type = 'text';
    shadow.appendChild(crInputNested);

    const actionButton = document.body.querySelector('.action-button');

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
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body"><button autofocus>button</button></div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectNotEquals(dialog, document.activeElement);
    expectEquals(button, document.activeElement);
  });

  // Ensuring that intersectionObserver does not fire any callbacks before the
  // dialog has been opened.
  test('body scrollable border not added before modal shown', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">body</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    assertFalse(dialog.open);
    const bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);
    const topShadow = dialog.$$('#cr-container-shadow-top');
    assertTrue(!!topShadow);
    const bottomShadow = dialog.$$('#cr-container-shadow-bottom');
    assertTrue(!!bottomShadow);

    return test_util.flushTasks().then(() => {
      assertFalse(topShadow.classList.contains('has-shadow'));
      assertFalse(bottomShadow.classList.contains('has-shadow'));
    });
  });

  test('dialog body scrollable border when appropriate', function(done) {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
        <div slot="body">
          <div style="height: 100px">tall content</div>
        </div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    const bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);
    const topShadow = dialog.$$('#cr-container-shadow-top');
    assertTrue(!!topShadow);
    const bottomShadow = dialog.$$('#cr-container-shadow-bottom');
    assertTrue(!!bottomShadow);

    dialog.showModal();  // Attach the dialog for the first time here.

    let observerCount = 0;

    // Needs to setup the observer before attaching, since InteractionObserver
    // calls callback before MutationObserver does.
    const observer = new MutationObserver(function(changes) {
      // Only care about class mutations.
      if (changes[0].attributeName != 'class') {
        return;
      }

      observerCount++;
      switch (observerCount) {
        case 1:  // Triggered when scrolled to bottom.
          assertFalse(bottomShadow.classList.contains('has-shadow'));
          assertTrue(topShadow.classList.contains('has-shadow'));
          bodyContainer.scrollTop = 0;
          break;
        case 2:  // Triggered when scrolled back to top.
          assertTrue(bottomShadow.classList.contains('has-shadow'));
          assertFalse(topShadow.classList.contains('has-shadow'));
          bodyContainer.scrollTop = 2;
          break;
        case 3:  // Triggered when finally scrolling to middle.
          assertTrue(bottomShadow.classList.contains('has-shadow'));
          assertTrue(topShadow.classList.contains('has-shadow'));
          observer.disconnect();
          done();
          break;
      }
    });
    observer.observe(topShadow, {attributes: true});
    observer.observe(bottomShadow, {attributes: true});

    // Height is normally set via CSS, but mixin doesn't work with innerHTML.
    bodyContainer.style.height = '60px';  // Element has "min-height: 60px".
    bodyContainer.scrollTop = 100;
  });

  test('dialog `open` attribute updated when Escape is pressed', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();

    assertTrue(dialog.open);
    assertTrue(dialog.hasAttribute('open'));

    const e = new CustomEvent('cancel', {cancelable: true});
    dialog.getNative().dispatchEvent(e);

    assertFalse(dialog.open);
    assertFalse(dialog.hasAttribute('open'));
  });

  test('dialog cannot be cancelled when `no-cancel` is set', function() {
    document.body.innerHTML = `
      <cr-dialog no-cancel>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();

    assertTrue(dialog.$.close.hidden);

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
    document.body.innerHTML = `
      <cr-dialog show-close-button>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    assertTrue(dialog.open);

    assertFalse(dialog.$.close.hidden);
    assertEquals('flex', window.getComputedStyle(dialog.$.close).display);
    dialog.$.close.click();
    assertFalse(dialog.open);
  });

  test('dialog close button hidden when showCloseButton is false', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();

    assertTrue(dialog.$.close.hidden);
    assertEquals('none', window.getComputedStyle(dialog.$.close).display);
  });

  test('keydown should be consumed when the property is true', function() {
    document.body.innerHTML = `
      <cr-dialog consume-keydown-event>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    assertTrue(dialog.open);
    assertTrue(dialog.consumeKeydownEvent);

    function assertKeydownNotReached() {
      assertNotReached('keydown event was propagated');
    }
    document.addEventListener('keydown', assertKeydownNotReached);

    return test_util.flushTasks().then(() => {
      MockInteractions.keyDownOn(dialog, 65, undefined, 'a');
      MockInteractions.keyDownOn(document.body, 65, undefined, 'a');
      document.removeEventListener('keydown', assertKeydownNotReached);
    });
  });

  test('keydown should be propagated when the property is false', function() {
    document.body.innerHTML = `
      <cr-dialog>
        <div slot="title">title</div>
      </cr-dialog>`;

    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    assertTrue(dialog.open);
    assertFalse(dialog.consumeKeydownEvent);

    let keydownCounter = 0;
    function assertKeydownCount() {
      keydownCounter++;
    }
    document.addEventListener('keydown', assertKeydownCount);

    return test_util.flushTasks().then(() => {
      MockInteractions.keyDownOn(dialog, 65, undefined, 'a');
      assertEquals(1, keydownCounter);
      document.removeEventListener('keydown', assertKeydownCount);
    });
  });

  test('show on attach', () => {
    document.body.innerHTML = `
      <cr-dialog show-on-attach>
        <div slot="title">title</div>
      </cr-dialog>`;
    const dialog = document.body.querySelector('cr-dialog');
    assertTrue(dialog.open);
  });
});
