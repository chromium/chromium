// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerAppElement, Router, UrlParam} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

suite('PasswordManagerAppTest', function() {
  let app: PasswordManagerAppElement;

  setup(function() {
    document.body.innerHTML = '';
    app = document.createElement('password-manager-app');
    document.body.appendChild(app);
    return flushTasks();
  });

  test('check layout', function() {
    assertTrue(isVisible(app));
    assertTrue(isVisible(app.$.sidebar));
    assertTrue(isVisible(app.$.toolbar));
  });

  test('UI search box updates URL parameters', function() {
    app.$.toolbar.$.mainToolbar.getSearchField().setValue('hello');

    assertEquals(
        'hello',
        String(Router.getInstance().currentRoute.queryParameters.get(
            UrlParam.SEARCH_TERM)));
  });

  test('URL parameters update UI search box', function() {
    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'test');
    Router.getInstance().updateRouterParams(query);
    assertEquals(
        'test', app.$.toolbar.$.mainToolbar.getSearchField().getValue());
  });
});
