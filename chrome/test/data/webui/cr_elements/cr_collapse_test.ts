// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('cr-collapse', function() {
  let collapse: CrCollapseElement;
  let child: HTMLElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    collapse = document.createElement('cr-collapse');
    document.body.appendChild(collapse);
    child = collapse.appendChild(document.createElement('div'));
    child.textContent = 'Hello World';
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
          parentOpened: Boolean,
        };
      }

      parentOpened: boolean = false;
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
});
