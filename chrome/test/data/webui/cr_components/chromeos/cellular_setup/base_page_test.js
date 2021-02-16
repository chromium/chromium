// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/base_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrComponentsBasePageTest', function() {
  let basePage;
  setup(function() {
    basePage = document.createElement('base-page');
    document.body.appendChild(basePage);
    Polymer.dom.flush();
  });

  test('Title is shown', function() {
    basePage.title = 'Base page titile';
    Polymer.dom.flush();
    const title = basePage.$$('#title');
    assertTrue(!!title);
  });

  test('Title is not shown', function() {
    const title = basePage.$$('#title');
    assertFalse(!!title);
  });

  test('Message icon is shown', function() {
    basePage.messageIcon = 'warning';
    Polymer.dom.flush();
    const messageIcon = basePage.$$('iron-icon');
    assertTrue(!!messageIcon);
    assertFalse(messageIcon.hidden);
  });

  test('Message icon is not shown', function() {
    const messageIcon = basePage.$$('iron-icon');
    assertTrue(!!messageIcon);
    assertTrue(messageIcon.hidden);
  });
});
