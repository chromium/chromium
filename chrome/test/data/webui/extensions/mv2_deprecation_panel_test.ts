// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-mv2-deprecation-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsMv2DeprecationPanelElement} from 'chrome://extensions/extensions.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo} from './test_util.js';

suite('ExtensionsMV2DeprecationPanel', function() {
  let panelElement: ExtensionsMv2DeprecationPanelElement;

  setup(function() {
    panelElement = document.createElement('extensions-mv2-deprecation-panel');
    panelElement.extensions = [createExtensionInfo()];
    document.body.appendChild(panelElement);
  });

  test('header content is always visible', function() {
    assertTrue(
        isVisible(panelElement.shadowRoot!.querySelector('.header-text')));
    assertTrue(
        isVisible(panelElement.shadowRoot!.querySelector('.header-button')));
  });
});
