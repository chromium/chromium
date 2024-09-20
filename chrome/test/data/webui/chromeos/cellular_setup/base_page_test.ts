// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cellular_setup/base_page.js';

import type {BasePageElement} from 'chrome://resources/ash/common/cellular_setup/base_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsBasePageTest', function() {
  let basePage: BasePageElement;

  setup(function() {
    basePage = document.createElement('base-page');
    document.body.appendChild(basePage);
    flush();
  });

  test('Title is shown', function() {
    basePage.title = 'Base page titile';
    flush();
    const title = basePage.shadowRoot!.querySelector('#title');
    assertTrue(!!title);
  });

  test('Title is not shown', function() {
    const title = basePage.shadowRoot!.querySelector('#title');
    assertFalse(!!title);
  });

  test('Message icon is shown', function() {
    basePage.messageIcon = 'warning';
    flush();
    const messageIcon = basePage.shadowRoot!.querySelector('iron-icon');
    assertTrue(!!messageIcon);
    assertFalse(messageIcon.hidden);
  });

  test('Message icon is not shown', function() {
    const messageIcon = basePage.shadowRoot!.querySelector('iron-icon');
    assertTrue(!!messageIcon);
    assertTrue(messageIcon.hidden);
  });
});
