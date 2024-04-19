// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppLanguageSelectionItemElement} from 'chrome://os-settings/lazy_load.js';
import {Locale} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {isHidden, replaceBody} from '../../app_management/test_util.js';

suite('<app-language-selection-item>', () => {
  const listItemId = '#listItem';
  const ironIconTag = 'iron-icon' as const;
  let appLanguageSelectionItem: AppLanguageSelectionItemElement;

  setup(() => {
    appLanguageSelectionItem =
        document.createElement('app-language-selection-item');

    replaceBody(appLanguageSelectionItem);
    flushTasks();
  });

  teardown(() => {
    appLanguageSelectionItem.remove();
  });

  function assertItem(expectedText: string, isSelected: boolean): void {
    const item = appLanguageSelectionItem.shadowRoot!.querySelector(listItemId);
    assertTrue(!!item, '#list-item not found');
    assertTrue(!!item.textContent, 'Item textContent not found');
    assertTrue(
        item.textContent.includes(expectedText),
        `Invalid text ${item.textContent}`);
    if (isSelected) {
      assertTrue(
          isVisible(item.querySelector(ironIconTag)),
          'Selection icon is not visible');
    } else {
      assertTrue(
          isHidden(item.querySelector(ironIconTag)),
          'Selection icon is not hidden');
    }
  }

  test('Display name and native display name is different, concat text', () => {
    const testDisplayName = 'testDisplayName';
    const testNativeDisplayName = 'testNativeDisplayName';
    appLanguageSelectionItem.item = {
      localeTag: 'test123',
      displayName: testDisplayName,
      nativeDisplayName: testNativeDisplayName,
    } as const satisfies Locale;
    appLanguageSelectionItem.selected = false;

    assertItem(
        testDisplayName + ' - ' + testNativeDisplayName,
        /* isSelected= */ false);
  });

  test('Display name and native display name is same, use display name', () => {
    const sameDisplayName = 'sameDisplayName';
    appLanguageSelectionItem.item = {
      localeTag: 'test123',
      displayName: sameDisplayName,
      nativeDisplayName: sameDisplayName,
    } as const satisfies Locale;
    appLanguageSelectionItem.selected = false;

    assertItem(sameDisplayName, /* isSelected= */ false);
  });

  test(
      'Display name exists and native display name is empty, use display name',
      () => {
        const testDisplayName = 'testDisplayName';
        appLanguageSelectionItem.item = {
          localeTag: 'test123',
          displayName: testDisplayName,
          nativeDisplayName: '',
        } as const satisfies Locale;
        appLanguageSelectionItem.selected = false;

        assertItem(testDisplayName, /* isSelected= */ false);
      });

  test('Display name and native display name is empty, use locale tag', () => {
    const testLocaleTag = 'test123';
    appLanguageSelectionItem.item = {
      localeTag: 'test123',
      displayName: '',
      nativeDisplayName: '',
    } as const satisfies Locale;
    appLanguageSelectionItem.selected = false;

    assertItem(testLocaleTag, /* isSelected= */ false);
  });

  test('Item is selected, selection icon should be visible', () => {
    const testDisplayName = 'testDisplayName';
    appLanguageSelectionItem.item = {
      localeTag: 'test123',
      displayName: testDisplayName,
      nativeDisplayName: '',
    } as const satisfies Locale;
    appLanguageSelectionItem.selected = true;

    assertItem(testDisplayName, /* isSelected= */ true);
  });
});
