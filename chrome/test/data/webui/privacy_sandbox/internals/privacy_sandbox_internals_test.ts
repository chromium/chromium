// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://privacy-sandbox-internals/mojo_timestamp.js';
import 'chrome://privacy-sandbox-internals/mojo_timedelta.js';
import 'chrome://privacy-sandbox-internals/value_display.js';

import {timestampLogicalFn, ValueDisplayElement} from 'chrome://privacy-sandbox-internals/value_display.js';
import {DictionaryValue, ListValue, Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

// Test that the <mojo-timestamp> CustomElement renders the correct time.
suite('MojoTimestampElementTest', function() {
  let tsElement: HTMLElement;

  suiteSetup(async function() {
    await customElements.whenDefined('mojo-timestamp');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tsElement = document.createElement('mojo-timestamp');
    document.body.appendChild(tsElement);
  });

  const testTime = (ts: string, rendered: string) => {
    tsElement.setAttribute('ts', ts);
    const time = tsElement.shadowRoot!.querySelector('#time');
    assertTrue(!!time);
    assertEquals(time.textContent, rendered);
  };

  test('epoch', async () => {
    testTime('0', 'epoch');
  });

  test('nearEpoch', async () => {
    testTime('1', 'Mon, 01 Jan 1601 00:00:00 GMT');
    testTime('1000000', 'Mon, 01 Jan 1601 00:00:01 GMT');
  });

  test('aroundNow', async () => {
    testTime('13348693565232806', 'Tue, 02 Jan 2024 18:26:05 GMT');
  });
});

// Test that the <mojo-timedelta> CustomElement renders the correct duration.
suite('MojoTimedeltaElementTest', function() {
  let element: HTMLElement;

  suiteSetup(async function() {
    await customElements.whenDefined('mojo-timedelta');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('mojo-timedelta');
    document.body.appendChild(element);
  });

  const testDuration = (duration: string, rendered: string) => {
    element.setAttribute('duration', duration);
    const time = element.shadowRoot!.querySelector('#duration');
    assertTrue(!!time);
    assertEquals(time.textContent, rendered);
  };

  test('zero', async () => {
    testDuration('0', '0 microseconds');
  });

  test('nonZero', async () => {
    testDuration('213', '213 microseconds');
    testDuration('123456123456123456', '123456123456123456 microseconds');
  });
});

// Test the <value-display> element.
suite('ValueDisplayElementTest', function() {
  let v: Value;
  let valueElement: ValueDisplayElement;

  suiteSetup(async function() {
    await customElements.whenDefined('value-display');
  });

  setup(function() {
    v = {} as Value;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    valueElement = document.createElement('value-display');
    document.body.appendChild(valueElement);
  });

  const assertType = (s: string) => {
    const span = valueElement.$('#type');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const assertValue = (s: string) => {
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  test('null', async () => {
    v.nullValue = 1;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'null');
    assertTrue(span.classList.contains('none'));
    assertType('');
  });

  test('trueBool', async () => {
    v.boolValue = true;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'true');
    assertTrue(span.classList.contains('bool-true'));
    assertType('');
  });

  test('falseBool', async () => {
    v.boolValue = false;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'false');
    assertTrue(span.classList.contains('bool-false'));
    assertType('');
  });

  test('int', async () => {
    v.intValue = 867;
    valueElement.configure(v);
    assertValue('867');
    assertType('(int)');
  });

  test('string', async () => {
    v.stringValue = 'all the small things';
    valueElement.configure(v);
    assertValue('all the small things');
    assertType('(string)');

    v.stringValue = '1234';
    valueElement.configure(v);
    assertValue('1234');
    assertType('(string)');
  });

  test('stringTimestamp', async () => {
    v.stringValue = '12345';
    valueElement.configure(v, timestampLogicalFn);
    assertValue('12345');
    assertType('(string)');
    const span = valueElement.$('#logical-value');
    assertTrue(!!span);
    assertTrue(span.classList.contains('defined'));
    const mojoTs = span.querySelector('mojo-timestamp');
    assertTrue(!!mojoTs);
    assertEquals(mojoTs.getAttribute('ts'), '12345');
  });

  test('list', async () => {
    v.listValue = {} as ListValue;
    v.listValue.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });
    valueElement.configure(v);
    assertValue(
        '[{"intValue":1},{"intValue":2},{"intValue":3},{"intValue":4}]');
    assertType('(list)');
  });

  test('dictionary', async () => {
    v.dictionaryValue = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    v.dictionaryValue.storage = {'v1': v1, 'v2': v2};
    valueElement.configure(v);
    assertValue('{"v1":{"intValue":10},"v2":{"stringValue":"bikes"}}');
    assertType('(dictionary)');
  });

  test('binary', async () => {
    v.binaryValue = [10, 20, 30, 40];
    valueElement.configure(v);
    assertValue('[10,20,30,40]');
    assertType('(binary)');
  });
});
