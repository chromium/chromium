// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerAppElement} from 'chrome://password-manager/password_manager.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PasswordManagerAppTest', function() {
  let app: PasswordManagerAppElement;

  setup(function() {
    document.body.innerHTML = '';
    app = document.createElement('password-manager-app');
    document.body.appendChild(app);
  });

  test('check layout', function() {
    assertTrue(isVisible(app));
    assertTrue(isVisible(app.$.sidebar));
    assertTrue(isVisible(app.$.toolbar));
  });
});
