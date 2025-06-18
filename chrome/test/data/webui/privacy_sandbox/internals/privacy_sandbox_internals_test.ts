// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/mojo_timestamp.js';
import 'chrome://privacy-sandbox-internals/mojo_timedelta.js';
import 'chrome://privacy-sandbox-internals/value_display.js';
import 'chrome://privacy-sandbox-internals/pref_display.js';
import 'chrome://privacy-sandbox-internals/internals_page.js';

import type {InternalsPage} from 'chrome://privacy-sandbox-internals/internals_page.js';
import type {PrefDisplayElement} from 'chrome://privacy-sandbox-internals/pref_display.js';
import type {ValueDisplayElement} from 'chrome://privacy-sandbox-internals/value_display.js';
import {timestampLogicalFn} from 'chrome://privacy-sandbox-internals/value_display.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DictionaryValue, ListValue, Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

function waitForElement(
    root: ShadowRoot, selector: string): Promise<HTMLElement> {
  return new Promise(resolve => {
    const check = () => {
      const element = root.querySelector<HTMLElement>(selector);
      if (element) {
        resolve(element);
      } else {
        requestAnimationFrame(check);
      }
    };
    check();
  });
}

// Test the <internals-page> element with the real PageHandler.
suite('InternalsPageTest', function() {
  let internalsPage: InternalsPage;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    internalsPage = document.createElement('internals-page');
    document.body.appendChild(internalsPage);
  });

  test('rendersAdvertisingPrefs', async () => {
    const firstPrefElement = await waitForElement(
        internalsPage.shadowRoot!, '#advertising-prefs > pref-display');
    assertTrue(
        !!firstPrefElement,
        'A <pref-display> element should be displayed for Advertising Prefs.');
  });

  test('rendersTrackingProtectionPrefs', async () => {
    const firstPrefElement = await waitForElement(
        internalsPage.shadowRoot!, '#tracking-protection-prefs > pref-display');
    assertTrue(
        !!firstPrefElement,
        'A <pref-display> element should be displayed for Tracking Protection Prefs.');
  });

  test('rendersTpcdExperimentPrefs', async () => {
    const firstPrefElement = await waitForElement(
        internalsPage.shadowRoot!, '#tpcd-experiment-prefs > pref-display');
    assertTrue(
        !!firstPrefElement,
        'A <pref-display> element should be displayed for TPCD Experiment Prefs.');
  });
});

// Tests the <internals-page> element's display of the TPCD tab based on feature
// flag status.
suite('PSInternalsPageTpcdTabLoadingTest', function() {
  let internalsPage: InternalsPage;

  function setShouldShowTpcdMetadataGrants(isEnabled: boolean) {
    loadTimeData.overrideValues({
      isPrivacySandboxInternalsDevUIEnabled: isEnabled,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    internalsPage = document.createElement('internals-page');
    document.body.appendChild(internalsPage);
  }

  async function findTpcdTab() {
    const tabBox = await waitForElement(internalsPage.shadowRoot!, '#ps-page');
    if (!tabBox) {
      return false;
    }

    const tabs = tabBox.querySelectorAll<HTMLElement>('div[slot="tab"]');
    const foundTab = Array.from(tabs).find(
        (tab: HTMLElement) =>
            tab.textContent?.trim() === 'TPCD_METADATA_GRANTS');

    return foundTab;
  }

  test('NoTpcdPanelIfDisabled', async () => {
    setShouldShowTpcdMetadataGrants(false);
    await internalsPage.whenLoaded;
    const anotherPanel = await waitForElement(
        internalsPage.shadowRoot!, 'div[slot="panel"][title="COOKIES"]');
    assertTrue(
        !!anotherPanel,
        'Panels that are not TPCD Metadata Grants should render normally.');
    await new Promise(resolve => setTimeout(resolve, 0));
    const tpcdPanel = internalsPage.shadowRoot!.querySelector(
        'div[slot="panel"][title="TPCD_METADATA_GRANTS"]');
    assertNull(
        tpcdPanel,
        'The panel for TPCD Metadata Grants should not exist when the flag is disabled.');
  });

  test('hidesTpcdMetadataGrantsTab', async () => {
    setShouldShowTpcdMetadataGrants(false);
    const tpcdTab = await findTpcdTab();
    assertFalse(
        !!tpcdTab, 'The TPCD tab should not exist when its flag is disabled.');
  });

  test('rendersTpcdMetadataGrantsTab', async () => {
    setShouldShowTpcdMetadataGrants(true);
    const tpcdTab = await findTpcdTab();
    assertTrue(
        !!tpcdTab, 'The TPCD tab should exist when its flag is enabled.');
  });
});

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

  test('epoch', () => {
    testTime('0', 'epoch');
  });

  test('nearEpoch', () => {
    testTime('1', 'Mon, 01 Jan 1601 00:00:00 GMT');
    testTime('1000000', 'Mon, 01 Jan 1601 00:00:01 GMT');
  });

  test('aroundNow', () => {
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

  test('zero', () => {
    testDuration('0', '0 microseconds');
  });

  test('nonZero', () => {
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

  const assertJsonValue = (s: string) => {
    const jsonValueElement = valueElement.$('#json-value');
    assertTrue(!!jsonValueElement);
    assertEquals(jsonValueElement.textContent, s);
  };

  test('null', () => {
    v.nullValue = 1;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'null');
    assertTrue(span.classList.contains('none'));
    assertType('');
  });

  test('trueBool', () => {
    v.boolValue = true;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'true');
    assertTrue(span.classList.contains('bool-true'));
    assertType('');
  });

  test('falseBool', () => {
    v.boolValue = false;
    valueElement.configure(v);
    const span = valueElement.$('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'false');
    assertTrue(span.classList.contains('bool-false'));
    assertType('');
  });

  test('int', () => {
    v.intValue = 867;
    valueElement.configure(v);
    assertValue('867');
    assertType('(int)');
  });

  test('string', () => {
    v.stringValue = 'all the small things';
    valueElement.configure(v);
    assertValue('all the small things');
    assertType('(string)');

    v.stringValue = '1234';
    valueElement.configure(v);
    assertValue('1234');
    assertType('(string)');
  });

  test('stringTimestamp', () => {
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

  test('list', () => {
    v.listValue = {} as ListValue;
    v.listValue.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });
    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        [{'intValue': 1}, {'intValue': 2}, {'intValue': 3}, {'intValue': 4}],
        null, 2));
    assertType('(list)');
  });

  test('dictionary', () => {
    v.dictionaryValue = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    v.dictionaryValue.storage = {'v1': v1, 'v2': v2};
    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        {'v1': {'intValue': 10}, 'v2': {'stringValue': 'bikes'}}, null, 2));
    assertType('(dictionary)');
  });

  test('flattens list with nested dictionary', () => {
    const vDict = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.stringValue = 'bikes';
    vDict.storage = {'v1': v1, 'v2': v2};

    v.listValue = {} as ListValue;
    v.listValue.storage = [
      {dictionaryValue: vDict} as Value,
    ];

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        [{'v1': {'intValue': 10}, 'v2': {'stringValue': 'bikes'}}], null, 2));
    assertType('(list)');
  });

  test('flattens dictionary with nested list', () => {
    const vList = {} as ListValue;
    vList.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });

    v.dictionaryValue = {} as DictionaryValue;
    v.dictionaryValue.storage = {
      'someKey': {listValue: vList} as Value,
    };

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        {
          'someKey': [
            {'intValue': 1},
            {'intValue': 2},
            {'intValue': 3},
            {'intValue': 4},
          ],
        },
        null, 2));
    assertType('(dictionary)');
  });

  test('binary', () => {
    v.binaryValue = [10, 20, 30, 40];
    valueElement.configure(v);
    assertValue('[10,20,30,40]');
    assertType('(binary)');
  });
});

// Test the <pref-display> element.
suite('PrefDisplayElementTest', function() {
  let v: Value;
  let prefDisplay: PrefDisplayElement;

  suiteSetup(async function() {
    await customElements.whenDefined('pref-display');
    await customElements.whenDefined('value-display');
  });

  setup(function() {
    v = {} as Value;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefDisplay = document.createElement('pref-display');
    document.body.appendChild(prefDisplay);
  });

  const assertName = (s: string) => {
    const span = prefDisplay.$('.id-pref-name');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const getValueElementOrFail = () => {
    const span = prefDisplay.$('.id-pref-value');
    assertTrue(!!span);
    const value = span.querySelector('value-display');
    assertTrue(!!value);
    return value;
  };

  const assertType = (s: string) => {
    const vElem = getValueElementOrFail();
    const type = vElem.$('#type');
    assertTrue(!!type);
    assertEquals(type.textContent, s);
  };

  const assertValue = (s: string) => {
    const vElem = getValueElementOrFail();
    const value = vElem.$('#value');
    assertTrue(!!value);
    assertEquals(value.textContent, s);
  };

  const getLogicalValueElementOrFail = () => {
    const vElem = getValueElementOrFail();
    const value = vElem.$('#logical-value');
    assertTrue(!!value);
    return value;
  };

  test('basicStringPref', () => {
    v.stringValue = 'this is a string';
    prefDisplay.configure('foo', v);
    assertName('foo');

    assertType('(string)');
    assertValue('this is a string');
    assertEquals(getLogicalValueElementOrFail().children.length, 0);
  });

  test('basicIntPref', () => {
    v.intValue = 100;
    prefDisplay.configure('some.int', v);
    assertName('some.int');

    assertType('(int)');
    assertValue('100');
    assertEquals(getLogicalValueElementOrFail().children.length, 0);
  });

  test('logicalStringPref', () => {
    v.stringValue = '12345';
    prefDisplay.configure('some.timestamp', v, timestampLogicalFn);
    assertName('some.timestamp');

    assertType('(string)');
    assertValue('12345');
    const value = getLogicalValueElementOrFail();
    assertEquals(value.children.length, 1);
    const mojoTs = value.querySelector('mojo-timestamp');
    assertTrue(!!mojoTs);
    assertEquals(mojoTs.getAttribute('ts'), '12345');
  });
});
