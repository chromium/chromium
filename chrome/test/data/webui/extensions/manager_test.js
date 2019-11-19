// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {navigation, Page} from 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from '../test_util.m.js';

window.extension_manager_tests = {};
extension_manager_tests.suiteName = 'ExtensionManagerTest';
/** @enum {string} */
extension_manager_tests.TestNames = {
  ChangePages: 'change pages',
  ItemListVisibility: 'item list visibility',
  SplitItems: 'split items',
  PageTitleUpdate: 'updates the title based on current route',
};

function getDataByName(list, name) {
  return assert(list.find(function(el) {
    return el.name == name;
  }));
}

suite(extension_manager_tests.suiteName, function() {
  /** @type {Manager} */
  let manager;

  /** @param {string} viewElement */
  function assertViewActive(tagName) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  setup(function() {
    PolymerTest.clearBody();
    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait for the first view to be active before starting tests, if one is
    // not active already. Sometimes, on Mac with native HTML imports
    // disabled, no views are active at this point.
    return manager.$.viewManager.querySelector('.active') ?
        Promise.resolve() :
        eventToPromise('view-enter-start', manager);
  });

  test(
      assert(extension_manager_tests.TestNames.ItemListVisibility), function() {
        const extension = getDataByName(manager.extensions_, 'My extension 1');

        const list = manager.$['items-list'];
        const listHasItemWithName = (name) =>
            !!list.extensions.find(el => el.name == name);

        expectEquals(manager.extensions_, manager.$['items-list'].extensions);
        expectTrue(listHasItemWithName('My extension 1'));

        manager.removeItem_(extension.id);
        flush();
        expectFalse(listHasItemWithName('My extension 1'));

        manager.addItem_('extensions_', extension);
        flush();
        expectTrue(listHasItemWithName('My extension 1'));
      });

  test(assert(extension_manager_tests.TestNames.SplitItems), function() {
    const sectionHasItemWithName = function(section, name) {
      return !!manager[section].find(function(el) {
        return el.name == name;
      });
    };

    // Test that we properly split up the items into two sections.
    expectTrue(sectionHasItemWithName('extensions_', 'My extension 1'));
    expectTrue(sectionHasItemWithName(
        'apps_', 'Platform App Test: minimal platform app'));
    expectTrue(sectionHasItemWithName('apps_', 'hosted_app'));
    expectTrue(sectionHasItemWithName('apps_', 'Packaged App Test'));
  });

  test(assert(extension_manager_tests.TestNames.ChangePages), function() {
    manager.$$('extensions-toolbar').$$('cr-toolbar').$$('#menuButton').click();
    flush();

    // We start on the item list.
    manager.$$('#sidebar').$['sections-extensions'].click();
    flush();
    assertViewActive('extensions-item-list');

    // Switch: item list -> keyboard shortcuts.
    manager.$$('#sidebar').$['sections-shortcuts'].click();
    flush();
    assertViewActive('extensions-keyboard-shortcuts');

    // Switch: item list -> detail view.
    const item = manager.$['items-list'].$$('extensions-item');
    assert(item);
    item.onDetailsTap_();
    flush();
    assertViewActive('extensions-detail-view');

    // Switch: detail view -> keyboard shortcuts.
    manager.$$('#sidebar').$['sections-shortcuts'].click();
    flush();
    assertViewActive('extensions-keyboard-shortcuts');

    // We get back on the item list.
    manager.$$('#sidebar').$['sections-extensions'].click();
    flush();
    assertViewActive('extensions-item-list');
  });

  test(assert(extension_manager_tests.TestNames.PageTitleUpdate), function() {
    expectEquals('Extensions', document.title);

    // Open details view with a valid ID.
    navigation.navigateTo(
        {page: Page.DETAILS, extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'});
    flush();
    expectEquals('Extensions - My extension 1', document.title);

    // Navigate back to the list view and check the page title.
    navigation.navigateTo({page: Page.LIST});
    flush();
    expectEquals('Extensions', document.title);
  });
});
