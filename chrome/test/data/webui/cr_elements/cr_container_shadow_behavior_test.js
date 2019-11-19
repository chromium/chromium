// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
// #import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrContainerShadowBehavior', function() {
  suiteSetup(function() {
    document.body.innerHTML = `
      <dom-module id="test-element">
        <template>
          <style>
            #container {
              height: 50px;
            }
          </style>
          <div id="before"></div>
          <div id="container" show-bottom-shadow$="[[showBottomShadow]]"></div>
          <div id="after"></div>
        </template>
      </dom-module>
    `;

    Polymer({
      is: 'test-element',

      properties: {
        showBottomShadow: Boolean,
      },

      behaviors: [CrContainerShadowBehavior],
    });
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  test('no bottom shadow', function() {
    const element = document.createElement('test-element');
    document.body.appendChild(element);

    // Should not have a bottom shadow div.
    assertFalse(!!element.$$('#cr-container-shadow-bottom'));
    assertTrue(!!element.$$('#cr-container-shadow-top'));

    element.showBottomShadow = true;

    // Still no bottom shadow since this is only checked in attached();
    assertFalse(!!element.$$('#cr-container-shadow-bottom'));
    assertTrue(!!element.$$('#cr-container-shadow-top'));
  });

  test('show bottom shadow', function() {
    const element = document.createElement('test-element');
    element.showBottomShadow = true;
    document.body.appendChild(element);

    // Has both shadows.
    assertTrue(!!element.$$('#cr-container-shadow-bottom'));
    assertTrue(!!element.$$('#cr-container-shadow-top'));
  });
});
