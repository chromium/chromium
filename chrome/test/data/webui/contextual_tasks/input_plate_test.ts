// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';

import type {ContextualTasksComposeboxElement} from 'chrome://contextual-tasks/composebox.js';
import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('InputPlateTest', () => {
  let composebox: ContextualTasksComposeboxElement;
  let proxy: TestExtensionBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      isZeroState: false,
    });

    proxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(proxy);

    await import(
        'chrome://contextual-tasks/contextual_tasks_extension/input_plate.js');

    composebox = document.createElement('contextual-tasks-composebox');
    composebox.setAttribute('is-extension', '');
    document.body.appendChild(composebox);

    await microtasksFinished();
  });

  test('Component renders', () => {
    assertTrue(isVisible(composebox));
  });
});
