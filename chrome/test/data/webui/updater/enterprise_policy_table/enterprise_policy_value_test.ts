// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/enterprise_policy_table/enterprise_policy_value.js';

import type {EnterprisePolicyValueElement} from 'chrome://updater/enterprise_policy_table/enterprise_policy_value.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EnterprisePolicyValueTest', () => {
  let element: EnterprisePolicyValueElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('enterprise-policy-value');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  suite('LastCheckPeriod', () => {
    test('formats correctly', async () => {
      element.policyName = 'LastCheckPeriod';
      element.value = 93784000000;
      await microtasksFinished();
      assertEquals(
          '1 day, 2 hours, 3 minutes, 4 seconds',
          element.shadowRoot.textContent);
    });

    test('handles invalid number', async () => {
      element.policyName = 'LastCheckPeriod';
      element.value = 'not a number';
      await microtasksFinished();
      assertEquals('not a number', element.shadowRoot.textContent);
    });
  });

  suite('UpdatesSuppressed', () => {
    test('formats correctly', async () => {
      element.policyName = 'UpdatesSuppressed';
      element.value = {
        'StartHour': 10,
        'StartMinute': 30,
        'Duration': 90,
      };
      await microtasksFinished();
      assertEquals('10:30 AM – 12:00 PM', element.shadowRoot.textContent);
    });

    test('overnight range', async () => {
      element.policyName = 'UpdatesSuppressed';
      element.value = {
        'StartHour': 23,
        'StartMinute': 0,
        'Duration': 120,
      };
      await microtasksFinished();
      assertEquals('11:00 PM – 1:00 AM', element.shadowRoot.textContent);
    });

    test('handles invalid object', async () => {
      element.policyName = 'UpdatesSuppressed';
      element.value = 'not an object';
      await microtasksFinished();
      assertEquals('not an object', element.shadowRoot.textContent);
    });

    test('handles missing fields', async () => {
      element.policyName = 'UpdatesSuppressed';
      element.value = {
        'StartHour': 10,
      };
      await microtasksFinished();
      assertEquals('{"StartHour":10}', element.shadowRoot.textContent);
    });
  });

  suite('PackageCacheSizeLimit', () => {
    test('formats correctly', async () => {
      element.policyName = 'PackageCacheSizeLimit';
      element.value = 1024;
      await microtasksFinished();
      assertEquals('1,024 MB', element.shadowRoot.textContent);
    });

    test('handles invalid number', async () => {
      element.policyName = 'PackageCacheSizeLimit';
      element.value = 'abc';
      await microtasksFinished();
      assertEquals('abc', element.shadowRoot.textContent);
    });
  });

  suite('PackageCacheExpires', () => {
    test('formats correctly', async () => {
      element.policyName = 'PackageCacheExpires';
      element.value = 30;
      await microtasksFinished();
      assertEquals('30 days', element.shadowRoot.textContent);
    });

    test('handles invalid number', async () => {
      element.policyName = 'PackageCacheExpires';
      element.value = 'xyz';
      await microtasksFinished();
      assertEquals('xyz', element.shadowRoot.textContent);
    });
  });

  suite('policies without specific formatters', () => {
    test('displays string value', async () => {
      element.value = 'hello';
      await microtasksFinished();
      assertEquals('hello', element.shadowRoot.textContent);
    });

    test('displays number value', async () => {
      element.value = 123;
      await microtasksFinished();
      assertEquals('123', element.shadowRoot.textContent);
    });

    test('displays JSON value', async () => {
      element.value = {foo: 'bar'};
      await microtasksFinished();
      assertEquals('{"foo":"bar"}', element.shadowRoot.textContent);
    });
  });
});
