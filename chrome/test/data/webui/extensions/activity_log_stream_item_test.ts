// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-stream-item. */

import type {ActivityLogStreamItemElement, StreamItem} from 'chrome://extensions/extensions.js';
import {ARG_URL_PLACEHOLDER} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {testVisible} from './test_util.js';

suite('ExtensionsActivityLogStreamItemTest', function() {
  /**
   * Extension activityLogStreamItem created before each test.
   */
  let activityLogStreamItem: ActivityLogStreamItemElement;
  let boundTestVisible: (selector: string, expectedVisible: boolean) => void;

  /**
   * StreamItem data for the activityLogStreamItem
   */
  let testStreamItem: StreamItem;

  // Initialize an activity log stream item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testStreamItem = {
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      pageUrl: '',
      argUrl: '',
      args: JSON.stringify([]),
      expanded: false,
    };

    activityLogStreamItem = document.createElement('activity-log-stream-item');
    activityLogStreamItem.data = testStreamItem;
    boundTestVisible = testVisible.bind(null, activityLogStreamItem);

    document.body.appendChild(activityLogStreamItem);
  });

  test(
      'item not expandable if it has no page URL, args or web request info',
      function() {
        flush();

        boundTestVisible('cr-expand-button', true);

        // Since |cr-expand-button| is always visible, we test that the
        // |cr-icon-button| within is not visible.
        boundTestVisible('cr-icon-button', false);
      });

  test(
      'page URL, args and web request info visible when item is expanded',
      function() {
        testStreamItem = {
          name: 'testAPI.testMethod',
          timestamp: 1550101623113,
          activityType:
              chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
          pageUrl: 'example.url',
          argUrl: '',
          args: JSON.stringify([null]),
          webRequestInfo: 'web request info',
          expanded: false,
        };

        activityLogStreamItem.set('data', testStreamItem);

        flush();
        boundTestVisible('cr-expand-button', true);
        activityLogStreamItem.shadowRoot!.querySelector(
                                             'cr-expand-button')!.click();

        flush();
        boundTestVisible('#page-url-link', true);
        boundTestVisible('#args-list', true);
        boundTestVisible('#web-request-section', true);
      });

  test('placeholder arg values are replaced by the argUrl', function() {
    const argUrl = 'arg.url';
    const placeholder = ARG_URL_PLACEHOLDER;
    // The <arg_url> placeholder except the '<' is escaped into a unicode
    // string to simulate the serializer on the C++ side.
    const escapedPlaceholder = '\\u003Carg_url>';

    testStreamItem = {
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      argUrl: argUrl,
      args: `[
        "${placeholder}",
        "${escapedPlaceholder}",
        ["${placeholder}"],
        {"url":"${escapedPlaceholder}"}
      ]`,
      expanded: false,
    };

    activityLogStreamItem.set('data', testStreamItem);

    flush();
    boundTestVisible('cr-expand-button', true);
    activityLogStreamItem.shadowRoot!.querySelector(
                                         'cr-expand-button')!.click();

    flush();
    boundTestVisible('#args-list', true);

    const argsDisplayed =
        activityLogStreamItem.shadowRoot!.querySelectorAll<HTMLElement>('.arg');
    assertEquals(4, argsDisplayed.length);

    assertEquals(`"${argUrl}"`, argsDisplayed[0]!.innerText!);
    assertEquals(`"${argUrl}"`, argsDisplayed[1]!.innerText!);
    assertEquals(`["${argUrl}"]`, argsDisplayed[2]!.innerText!);
    assertEquals(`{"url":"${argUrl}"}`, argsDisplayed[3]!.innerText!);
  });

  teardown(function() {
    activityLogStreamItem.remove();
  });
});
