// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_action_menu.js';

import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextualActionMenu', () => {
  let actionMenu: ContextualActionMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
      composeboxFileMaxCount: 10,
      composeboxShowContextMenuTabPreviews: true,
      composeboxShowPdfUpload: true,
    });

    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    Object.assign(
        actionMenu,
        {fileNum: 0, disabledTabIds: new Map(), tabSuggestions: []});
    document.body.appendChild(actionMenu);
    await microtasksFinished();
  });

  test('menu is hidden initially', async () => {
    await microtasksFinished();
    assertFalse(actionMenu.$.menu.open);
  });
});
