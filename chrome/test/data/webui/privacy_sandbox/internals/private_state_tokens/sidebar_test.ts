// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensSidebarElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SidebarTest', () => {
  let sidebar: PrivateStateTokensSidebarElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('private-state-tokens-sidebar');
    document.body.appendChild(sidebar);
  });

  test('check layout', () => {
    assertTrue(isVisible(sidebar));
    const renderedLinks = sidebar.shadowRoot!.querySelectorAll('a');
    assertEquals(1, renderedLinks.length);
    assertEquals('chrome://settings/', renderedLinks[0]!.href);
    assertEquals('Settings', renderedLinks[0]!.textContent!.trim());
  });
});
