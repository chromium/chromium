// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {html, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {TooltipPosition} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-tooltip', function() {
  let tooltip: CrTooltipElement;
  let parent: CrLitElement;

  // Test parent element.
  class TestElement extends CrLitElement {
    static get is() {
      return 'test-element';
    }

    override render() {
      return html`
        <div id="test-for"></div>
        <div id="test-manual"></div>
        <cr-tooltip animation-delay="0">
          <span id="tooltip-text"></span>
        </cr-tooltip>`;
    }
  }

  customElements.define(TestElement.is, TestElement);

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    parent = document.createElement('test-element') as TestElement;
    document.body.appendChild(parent);
    tooltip = parent.shadowRoot!.querySelector('cr-tooltip')!;
  });

  test('sets correct target', async() => {
    // The default target is the parent.
    assertEquals(parent, tooltip.target);
    tooltip.for = 'test-for';
    await microtasksFinished();
    assertEquals(parent.shadowRoot!.querySelector('#test-for'), tooltip.target);
    const manualDiv = parent.shadowRoot!.querySelector('#test-manual');
    assertTrue(!!manualDiv);
    tooltip.target = manualDiv;
    await microtasksFinished();
    assertEquals(manualDiv, tooltip.target);
  });

  test('show/hide', async() => {
    // Tooltip should not show since there is no text content.
    tooltip.show();
    await microtasksFinished();
    assertTrue(tooltip.$.tooltip.hidden);

    // Tooltip shows when show() is called and plays fade in animation.
    const text = parent.shadowRoot!.querySelector<HTMLElement>('#tooltip-text');
    assertTrue(!!text);
    text.textContent = 'test';
    tooltip.show();
    assertFalse(tooltip.$.tooltip.hidden);
    assertTrue(tooltip.$.tooltip.classList.contains('fade-in-animation'));
    assertFalse(tooltip.$.tooltip.classList.contains('fade-out-animation'));

    await eventToPromise('animationend', tooltip.$.tooltip);
    assertFalse(tooltip.$.tooltip.hidden);

    // Tooltip hides when hide() is called and plays fade out animation.
    tooltip.hide();
    assertTrue(tooltip.$.tooltip.classList.contains('fade-out-animation'));
    assertFalse(tooltip.$.tooltip.classList.contains('fade-in-animation'));
    assertFalse(tooltip.$.tooltip.hidden);
    await eventToPromise('animationend', tooltip.$.tooltip);
    assertTrue(tooltip.$.tooltip.hidden);

    // Tooltip shows when pointer enters the target.
    parent.dispatchEvent(
        new CustomEvent('pointerenter', {bubbles: true, composed: true}));
    await eventToPromise('animationend', tooltip.$.tooltip);
    assertFalse(tooltip.$.tooltip.hidden);

    // Tooltip hides when pointer leaves the target.
    parent.dispatchEvent(
        new CustomEvent('pointerleave', {bubbles: true, composed: true}));
    await eventToPromise('animationend', tooltip.$.tooltip);
    assertTrue(tooltip.$.tooltip.hidden);

    // If manual mode is enabled, does not respond to pointer events.
    tooltip.manualMode = true;
    await microtasksFinished();
    parent.dispatchEvent(
        new CustomEvent('pointerenter', {bubbles: true, composed: true}));
    await new Promise(resolve => setTimeout(resolve, 1));
    assertTrue(tooltip.$.tooltip.hidden);
  });

  test('positioning', async () => {
    const text = parent.shadowRoot!.querySelector<HTMLElement>('#tooltip-text');
    assertTrue(!!text);
    text.textContent = 'test';
    tooltip.for = 'test-for';
    await microtasksFinished();
    tooltip.show();

    const target = parent.shadowRoot!.querySelector('#test-for');
    assertTrue(!!target);
    const parentRect = parent.getBoundingClientRect();
    const targetRect = target.getBoundingClientRect();
    const tooltipRect = tooltip.getBoundingClientRect();
    const horizontalCenterOffset = (targetRect.width - tooltipRect.width) / 2;
    const verticalCenterOffset = (targetRect.height - tooltipRect.height) / 2;
    const targetLeft = targetRect.left - parentRect.left;
    const targetTop = targetRect.top - parentRect.top;

    let expectedLeft = targetLeft + horizontalCenterOffset;
    let expectedTop = targetTop + targetRect.height + 14;  // default offset 14

    assertEquals(
        expectedLeft,
        (tooltip.computedStyleMap().get('left') as CSSUnitValue).value);
    assertEquals(
        expectedTop,
        (tooltip.computedStyleMap().get('top') as CSSUnitValue).value);

    // Check that setting a new offset and updating position works as expected.
    tooltip.offset = 6;
    tooltip.updatePosition();
    await microtasksFinished();
    expectedTop = expectedTop - 8;  // 14 - 6
    assertEquals(
        expectedLeft,
        (tooltip.computedStyleMap().get('left') as CSSUnitValue).value);
    assertEquals(
        expectedTop,
        (tooltip.computedStyleMap().get('top') as CSSUnitValue).value);

    // Check that a different tooltip position works as expected.
    tooltip.position = TooltipPosition.LEFT;
    tooltip.updatePosition();
    await microtasksFinished();
    expectedLeft = targetLeft - tooltipRect.width - 6;  // Offset 6
    expectedTop = targetTop + verticalCenterOffset;
    assertEquals(
        expectedLeft,
        (tooltip.computedStyleMap().get('left') as CSSUnitValue).value);
    assertEquals(
        expectedTop,
        (tooltip.computedStyleMap().get('top') as CSSUnitValue).value);
  });
});

suite('cr-tooltip in dialog', function() {
  let tooltip: CrTooltipElement;
  let parent: CrLitElement;

  // Test parent element.
  class TestDialogElement extends CrLitElement {
    static get is() {
      return 'test-dialog-element';
    }

    override render() {
      return html`
        <cr-dialog show-on-attach>
          <div slot="body">
            <div id="test-for">Hover for tooltip</div>
            <cr-tooltip animation-delay="0" for="test-for"
                fit-to-visible-bounds>
              <span id="tooltip-text">Hello from cr-dialog slot</span>
            </cr-tooltip>
          </div>
        </cr-dialog>`;
    }
  }

  customElements.define(TestDialogElement.is, TestDialogElement);

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    parent = document.createElement('test-dialog-element') as TestDialogElement;
    document.body.appendChild(parent);
    tooltip = parent.shadowRoot!.querySelector('cr-tooltip')!;
    await microtasksFinished();
  });

  test('positioning', () => {
    tooltip.show();
    const dialog = parent.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog);
    const parentRect = dialog.$.dialog.getBoundingClientRect();
    const target = parent.shadowRoot!.querySelector('#test-for');
    assertTrue(!!target);
    const targetRect = target.getBoundingClientRect();
    const tooltipRect = tooltip.getBoundingClientRect();
    const horizontalCenterOffset = (targetRect.width - tooltipRect.width) / 2;
    const targetLeft = targetRect.left - parentRect.left;
    const targetTop =
        Math.max(targetRect.top - parentRect.top, -parentRect.top);
    const expectedLeft = targetLeft + horizontalCenterOffset;
    const expectedTop = targetTop + targetRect.height + 14;  // default offset
    assertEquals(
        expectedLeft,
        (tooltip.computedStyleMap().get('left') as CSSUnitValue).value);
    assertEquals(
        expectedTop,
        (tooltip.computedStyleMap().get('top') as CSSUnitValue).value);
  });
});
