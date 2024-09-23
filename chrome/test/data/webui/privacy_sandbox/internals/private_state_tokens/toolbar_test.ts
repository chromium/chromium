// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensToolbarElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ToolbarTest', function() {
  let toolbar: PrivateStateTokensToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('private-state-tokens-toolbar');
    document.body.appendChild(toolbar);
  });

  test('check layout', function() {
    assertTrue(isVisible(toolbar));
    // TODO(crbug.com/348590926): Add more tests later...
  });
});
