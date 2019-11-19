// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-dom-switch>', function() {
  let domSwitch;
  let domBind;

  setup(async function() {
    PolymerTest.clearBody();

    const template = `
        <dom-bind>
          <template>
            <app-management-dom-switch id="switch">
              <template>
                <div id='child1' route-id='1'>[[property.x]]</div>
                <div id='child2' route-id='2'>[[property.y]]</div>
              </template>
            </app-management-dom-switch>
          </template>
        </dom-bind>`;

    document.body.innerHTML = template;

    domSwitch = document.getElementById('switch');
    domBind = document.querySelector('dom-bind');
  });

  test('children are attached/detached when the route changes', function() {
    // No children are attached initially.
    assertFalse(!!document.getElementById('child1'));
    assertFalse(!!document.getElementById('child2'));

    // When a child is selected, it is attached to the DOM.
    domSwitch.route = '1';
    assertTrue(!!document.getElementById('child1'));
    assertFalse(!!document.getElementById('child2'));

    // When another child is selected, it is attached and the previous child
    // is detached.
    domSwitch.route = '2';
    assertFalse(!!document.getElementById('child1'));
    assertTrue(!!document.getElementById('child2'));

    // When no child is selected, the currently selected child is detached.
    domSwitch.route = null;
    assertFalse(!!document.getElementById('child1'));
    assertFalse(!!document.getElementById('child2'));
  });

  test('binding to properties and paths works', function() {
    // Bindings update when a parent property is changed.
    domBind.property = {x: 1, y: 2};

    domSwitch.route = '1';
    let child = document.getElementById('child1');
    assertEquals('1', child.textContent);

    domSwitch.route = '2';
    child = document.getElementById('child2');
    assertEquals('2', child.textContent);

    // Bindings update when a path of a parent property is changed.
    domBind.set('property.x', 3);
    domBind.set('property.y', 4);

    assertEquals('4', child.textContent);

    domSwitch.route = '1';
    child = document.getElementById('child1');
    assertEquals('3', child.textContent);
  });
});
