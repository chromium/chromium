// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-collapse', function() {
  let collapse: CrCollapseElement;
  let child: HTMLElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
       <cr-collapse>
         <div style="height: 20px">Hello World</div>
       </cr-collapse>`;
    collapse = document.body.querySelector('cr-collapse')!;
    child = document.body.querySelector('div')!;
  });

  function assertCollapsedState() {
    assertFalse(isVisible(collapse));
    assertFalse(isVisible(child));
    assertEquals('0px', window.getComputedStyle(collapse).maxHeight);
  }

  function assertExpandedState() {
    assertTrue(isVisible(collapse));
    assertTrue(isVisible(child));
    assertEquals('none', window.getComputedStyle(collapse).maxHeight);
  }

  test('does not animate on first render', async () => {
    const transitions: string[] = [];
    collapse.addEventListener('transitionend', e => {
      assertEquals('max-height', e.propertyName);
      transitions.push(
          (collapse.computedStyleMap().get('max-height') as CSSKeywordValue)
              .value);
    });

    // 310ms = default animation duration + 10ms.
    await new Promise(resolve => setTimeout(resolve, 310));
    assertEquals(0, transitions.length);
    collapse.opened = true;
    await eventToPromise('transitionend', collapse);
    assertEquals(1, transitions.length);
    assertEquals('none', transitions[0]);
  });

  test('open/close with property', async() => {
    assertFalse(collapse.opened);
    assertCollapsedState();

    collapse.opened = true;
    await eventToPromise('transitionend', collapse);
    assertExpandedState();

    collapse.opened = false;
    await eventToPromise('transitionend', collapse);
    assertCollapsedState();
  });

  test('open/close with methods', async() => {
    assertFalse(collapse.opened);
    assertCollapsedState();

    collapse.show();
    await eventToPromise('transitionend', collapse);
    assertTrue(collapse.opened);
    assertExpandedState();

    collapse.hide();
    await eventToPromise('transitionend', collapse);
    assertFalse(collapse.opened);
    assertCollapsedState();

    collapse.toggle();
    await eventToPromise('transitionend', collapse);
    assertTrue(collapse.opened);
    assertExpandedState();
  });

  // Test that 2-way bindings with Polymer parent elements work.
  test('TwoWayBindingWithPolymerParent', async () => {
    class TestPolymerElement extends PolymerElement {
      static get is() {
        return 'test-polymer-element';
      }

      static get template() {
        return html`
          <cr-collapse opened="{{parentOpened}}" no-animation>
             <div>Some content</div>
          </cr-collapse>`;
      }

      static get properties() {
        return {
          parentOpened: {
            type: Boolean,
            value: false,
          },
        };
      }

      declare parentOpened: boolean;
    }

    customElements.define(TestPolymerElement.is, TestPolymerElement);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element =
        document.createElement('test-polymer-element') as TestPolymerElement;
    document.body.appendChild(element);

    const collapse = element.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!collapse);
    const whenOpenedChanged = eventToPromise('opened-changed', collapse);
    collapse.toggle();
    await whenOpenedChanged;
    assertTrue(element.parentOpened);
  });

  test('customizes transition timing and durations', async () => {
    collapse.style.setProperty('--cr-collapse-expand-duration', '10ms');
    collapse.style.setProperty('--cr-collapse-collapse-duration', '5ms');
    collapse.style.setProperty(
        '--cr-collapse-transition-timing-function', 'linear');

    const collapsedStyle = getComputedStyle(collapse);
    assertEquals('0.005s', collapsedStyle.transitionDuration);
    assertEquals('linear', collapsedStyle.transitionTimingFunction);

    let whenTransitionEnd = eventToPromise('transitionend', collapse);
    collapse.opened = true;
    await microtasksFinished();
    const expandedStyle = getComputedStyle(collapse);
    assertEquals('0.01s', expandedStyle.transitionDuration);
    assertEquals('linear', expandedStyle.transitionTimingFunction);
    await whenTransitionEnd;
    assertExpandedState();

    whenTransitionEnd = eventToPromise('transitionend', collapse);
    collapse.opened = false;
    await microtasksFinished();
    assertEquals('0.005s', getComputedStyle(collapse).transitionDuration);
    await whenTransitionEnd;
    assertCollapsedState();
  });

  test('falls back to legacy duration variable', async () => {
    assertEquals('0.3s', getComputedStyle(collapse).transitionDuration);

    collapse.style.setProperty('--iron-collapse-transition-duration', '50ms');
    assertEquals('0.05s', getComputedStyle(collapse).transitionDuration);

    collapse.opened = true;
    await microtasksFinished();
    assertEquals('0.05s', getComputedStyle(collapse).transitionDuration);
  });

  test('handles zero duration transitions synchronously', async () => {
    collapse.style.setProperty('--cr-collapse-expand-duration', '0ms');
    collapse.style.setProperty('--cr-collapse-collapse-duration', '50ms');

    collapse.opened = true;
    await microtasksFinished();
    assertExpandedState();
    assertTrue(collapse.classList.contains('collapse-opened'));

    collapse.style.setProperty('--cr-collapse-collapse-duration', '0ms');
    collapse.opened = false;
    await microtasksFinished();
    assertCollapsedState();
    assertTrue(collapse.classList.contains('collapse-closed'));
  });

  test('ignores child and non-max-height transitionend', async () => {
    collapse.opened = true;
    await microtasksFinished();
    assertFalse(collapse.classList.contains('collapse-opened'));

    child.dispatchEvent(new TransitionEvent(
        'transitionend',
        {bubbles: true, composed: true, propertyName: 'max-height'}));
    assertFalse(collapse.classList.contains('collapse-opened'));

    collapse.dispatchEvent(
        new TransitionEvent('transitionend', {propertyName: 'opacity'}));
    assertFalse(collapse.classList.contains('collapse-opened'));

    await eventToPromise('transitionend', collapse);
    assertExpandedState();
    assertTrue(collapse.classList.contains('collapse-opened'));
  });

  test('animates with multi-property transition', async () => {
    collapse.style.transition = 'max-height 10ms linear, opacity 10ms linear';

    let whenTransitionEnd = eventToPromise('transitionend', collapse);
    collapse.opened = true;
    assertEquals('max-height', (await whenTransitionEnd).propertyName);
    assertExpandedState();
    assertTrue(collapse.classList.contains('collapse-opened'));

    whenTransitionEnd = eventToPromise('transitionend', collapse);
    collapse.opened = false;
    assertEquals('max-height', (await whenTransitionEnd).propertyName);
    assertCollapsedState();
    assertTrue(collapse.classList.contains('collapse-closed'));
  });

  test('handles non-max-height and overridden transitions', async () => {
    collapse.style.transition = 'opacity 10ms linear';
    collapse.opened = true;
    await microtasksFinished();
    assertExpandedState();
    assertTrue(collapse.classList.contains('collapse-opened'));

    collapse.style.transition = 'opacity 10ms linear, max-height 0ms';
    collapse.opened = false;
    await microtasksFinished();
    assertCollapsedState();
    assertTrue(collapse.classList.contains('collapse-closed'));

    collapse.style.transition = 'all 10ms linear, max-height 0ms';
    collapse.opened = true;
    await microtasksFinished();
    assertExpandedState();
    assertTrue(collapse.classList.contains('collapse-opened'));
  });
});
