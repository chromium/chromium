// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';

suite('ApnListTest', function() {
  /** @type {ApnListItemElement} */
  let apnListItem = null;

  setup(function() {
    apnListItem = document.createElement('apn-list-item');
    document.body.appendChild(apnListItem);
    flush();
  });

  test('Check if APN list item exists', async function() {
    assertTrue(!!apnListItem);
  });

  test('Check if connected sublabel is shown', async function() {
    apnListItem.isConnected = false;
    await flushTasks();

    const subLabel = apnListItem.shadowRoot.querySelector('#subLabel');
    assertTrue(!!subLabel);
    assertTrue(subLabel.hasAttribute('hidden'));
    apnListItem.isConnected = true;
    await flushTasks();

    assertFalse(subLabel.hasAttribute('hidden'));
    assertEquals(
        subLabel.innerText, apnListItem.i18n('NetworkHealthStateConnected'));
  });

  test('Check if APN three dot menu shows', async function() {
    apnListItem.apn = {name: 'apn1'};
    await flushTasks();

    const menuButton =
        apnListItem.shadowRoot.querySelector('#actionMenuButton');
    assertTrue(!!menuButton);
    assertFalse(apnListItem.$.dotsMenu.open);

    menuButton.click();
    await flushTasks();
    assertTrue(apnListItem.$.dotsMenu.open);
  });
});
