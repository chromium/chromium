// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementDomSwitchElement} from 'chrome://os-settings/lazy_load.js';
import {DomBind} from 'chrome://resources/polymer/v3_0/polymer/lib/elements/dom-bind.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<app-management-dom-switch>', () => {
  let domSwitch: AppManagementDomSwitchElement;

  interface BindData {
    property: {x: number, y: number};
  }
  type DomBindElement = DomBind&BindData;
  let domBind: DomBindElement;

  setup(() => {
    document.body.appendChild(html`
      <dom-bind>
        <template>
          <app-management-dom-switch id="switch">
            <template>
              <div id='child1' route-id='1'>[[property.x]]</div>
              <div id='child2' route-id='2'>[[property.y]]</div>
            </template>
          </app-management-dom-switch>
        </template>
      </dom-bind>
    `.content);

    const element =
        document.querySelector<AppManagementDomSwitchElement>('#switch');
    assertTrue(!!element);
    domSwitch = element;
    const bind = document.querySelector<DomBindElement>('dom-bind');
    assertTrue(!!bind);
    domBind = bind;
  });

  test('children are attached/detached when the route changes', () => {
    // No children are attached initially.
    assertNull(document.getElementById('child1'));
    assertNull(document.getElementById('child2'));

    // When a child is selected, it is attached to the DOM.
    domSwitch.route = '1';
    assertTrue(!!document.getElementById('child1'));
    assertNull(document.getElementById('child2'));

    // When another child is selected, it is attached and the previous child
    // is detached.
    domSwitch.route = '2';
    assertNull(document.getElementById('child1'));
    assertTrue(!!document.getElementById('child2'));

    // When no child is selected, the currently selected child is detached.
    domSwitch.route = null;
    assertNull(document.getElementById('child1'));
    assertNull(document.getElementById('child2'));
  });

  test('binding to properties and paths works', () => {
    // Bindings update when a parent property is changed.
    domBind.property = {x: 1, y: 2};

    domSwitch.route = '1';
    let child = document.getElementById('child1');
    assertTrue(!!child);
    assertEquals('1', child.textContent);

    domSwitch.route = '2';
    child = document.getElementById('child2');
    assertTrue(!!child);
    assertEquals('2', child.textContent);

    // Bindings update when a path of a parent property is changed.
    domBind.set('property.x', 3);
    domBind.set('property.y', 4);

    assertEquals('4', child.textContent);

    domSwitch.route = '1';
    child = document.getElementById('child1');
    assertTrue(!!child);
    assertEquals('3', child.textContent);
  });
});
