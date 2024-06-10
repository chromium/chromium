// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseRowMixin} from 'chrome://os-settings/os_settings.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../../utils.js';

const TestRowElementBase = BaseRowMixin(PolymerElement);
class TestRowElement extends TestRowElementBase {}
customElements.define('test-row-element', TestRowElement);

suite('SettingsBaseRowMixin', () => {
  let testRowElement: TestRowElement;
  const label = 'test label';
  const sublabel = 'test sublabel';
  const icon = 'test icon';
  const learnMoreUrl = 'chrome://os-settings/test';
  const ariaLabel = 'test aria label';
  const ariaDescription = 'test aria description';

  setup(async () => {
    clearBody();
    testRowElement =
        document.createElement('test-row-element') as TestRowElement;
    document.body.appendChild(testRowElement);
    await flushTasks();
  });

  suite('fundamental properties', () => {
    test('label is set correctly', () => {
      testRowElement.label = label;
      assertEquals(label, testRowElement.label);
    });

    test('sublabel is set correctly', () => {
      testRowElement.sublabel = sublabel;
      assertEquals(sublabel, testRowElement.sublabel);
    });

    test('icon is set correctly', () => {
      testRowElement.icon = icon;
      assertEquals(icon, testRowElement.icon);
    });

    test('learnMoreUrl reflects to attribute', () => {
      testRowElement.learnMoreUrl = learnMoreUrl;
      assertEquals(learnMoreUrl, testRowElement.learnMoreUrl);
      assertEquals(learnMoreUrl, testRowElement.getAttribute('learn-more-url'));
    });

    test('ariaLabel reflects to attribute', () => {
      testRowElement.ariaLabel = ariaLabel;
      assertEquals(ariaLabel, testRowElement.ariaLabel);
      assertEquals(ariaLabel, testRowElement.getAttribute('aria-label'));
    });

    test('ariaDescription reflects to attribute', () => {
      testRowElement.ariaDescription = ariaDescription;
      assertEquals(ariaDescription, testRowElement.ariaDescription);
      assertEquals(
          ariaDescription, testRowElement.getAttribute('aria-description'));
    });
  });

  suite('getAriaLabel()', () => {
    test('should return null if neither label nor ariaLabel is defined', () => {
      assertNull(testRowElement.getAriaLabel());
    });

    test('ariaLabel takes precedence when defined', () => {
      testRowElement.label = label;

      // `getAriaLabel` returns the value of `label` when `ariaLabel` is not
      // defined.
      assertEquals(label, testRowElement.getAriaLabel());

      testRowElement.ariaLabel = ariaLabel;
      // 'ariaLabel' takes precedence over `label` when it's set.
      assertEquals(ariaLabel, testRowElement.getAriaLabel());
    });
  });

  suite('getAriaDescription()', () => {
    test(
        'should return null if neither sublabel nor ariaDescription is defined',
        () => {
          assertNull(testRowElement.getAriaDescription());
        });

    test('ariaDescription takes precedence when defined', () => {
      testRowElement.sublabel = sublabel;

      // `getAriaDescription` returns the value of `sublabel` when
      // `ariaDescription` is not defined.
      assertEquals(sublabel, testRowElement.getAriaDescription());

      testRowElement.ariaDescription = ariaDescription;
      // 'ariaLabel' takes precedence over `label` when it's set.
      assertEquals(ariaDescription, testRowElement.getAriaDescription());
    });
  });
});
