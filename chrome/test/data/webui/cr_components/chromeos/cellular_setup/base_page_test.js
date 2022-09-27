// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cellular_setup/base_page.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('CrComponentsBasePageTest', function() {
  let basePage;
  setup(function() {
    basePage = document.createElement('base-page');
    document.body.appendChild(basePage);
    flush();
  });

  test('Title is shown', function() {
    basePage.title = 'Base page titile';
    flush();
    const title = basePage.$$('#title');
    assertTrue(!!title);
  });

  test('Title is not shown', function() {
    const title = basePage.$$('#title');
    assertFalse(!!title);
  });

  test('Message icon is shown', function() {
    basePage.messageIcon = 'warning';
    flush();
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
