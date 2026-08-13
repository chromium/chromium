// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://content-settings/app.js';
import 'chrome://content-settings/expandable_json_viewer.js';
import 'chrome://content-settings/mojo_timestamp.js';
import 'chrome://content-settings/mojo_timedelta.js';
import 'chrome://content-settings/value_display.js';

import type {AppElement} from 'chrome://content-settings/app.js';
import {browserProxyFactory} from 'chrome://content-settings/content_settings_internals.mojom-webui.js';
import type {ExpandableJsonViewerElement} from 'chrome://content-settings/expandable_json_viewer.js';
import {Router} from 'chrome://content-settings/router.js';
import type {ValueDisplayElement} from 'chrome://content-settings/value_display.js';
import type {DictionaryValue, ListValue, Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContentSettingsPageHandler} from './test_content_settings_browser_proxy.js';

// Test suite for FrameList behavior within the AppElement.
suite('ContentSettingsFrameListTest', function() {
  let page: AppElement;
  let shadowRoot: ShadowRoot;

  setup(async function() {
    const testHandler = new TestContentSettingsPageHandler();
    browserProxyFactory.setInstance({handler: testHandler});
    Router.resetInstanceForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('app-element');
    document.body.appendChild(page);
    shadowRoot = page.shadowRoot!;
    await microtasksFinished();
  });

  test('tabsAreAlphabeticallySorted', () => {
    const tabs =
        Array.from(shadowRoot.querySelectorAll<HTMLElement>('[slot="tab"]'));
    assertTrue(tabs.length > 0);
    const tabNames = tabs.map(tab => tab.innerText);
    const sortedTabNames = [...tabNames].sort((a, b) => a.localeCompare(b));
    assertEquals(JSON.stringify(tabNames), JSON.stringify(sortedTabNames));
  });
});

// Test suite for routing within the Content Settings page.
suite('ContentSettingsRoutingTest', function() {
  let page: AppElement;
  let shadowRoot: ShadowRoot;
  let tabContainer: HTMLElement;

  enum Page {
    COOKIES = 'cookies',
    JAVASCRIPT = 'javascript',
    POPUPS = 'popups',
  }

  setup(async function() {
    const testHandler = new TestContentSettingsPageHandler();
    browserProxyFactory.setInstance({handler: testHandler});

    Router.resetInstanceForTesting();
    window.history.replaceState({}, '', window.location.pathname);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('app-element');
    document.body.appendChild(page);
    shadowRoot = page.shadowRoot!;
    tabContainer = shadowRoot.querySelector<HTMLElement>('#cs-page')!;
    await microtasksFinished();
  });

  test('switchesTabOnClickAndUpdateUrl', async () => {
    const cookiesTab = shadowRoot.querySelector<HTMLElement>(
        `div[slot="tab"][data-page-name="${Page.COOKIES}"]`)!;
    cookiesTab.click();
    await microtasksFinished();

    const params = new URLSearchParams(window.location.search);
    assertEquals(Page.COOKIES, params.get('page'));
  });

  test('updatesTabWhenBackButtonIsUsed', async () => {
    Router.getInstance().navigateTo(Page.JAVASCRIPT);
    assertEquals(
        Page.JAVASCRIPT,
        new URLSearchParams(window.location.search).get('page'));

    Router.getInstance().navigateTo(Page.POPUPS);
    assertEquals(
        Page.POPUPS, new URLSearchParams(window.location.search).get('page'));

    const popstatePromise = eventToPromise('popstate', window);
    history.back();
    await popstatePromise;
    await microtasksFinished();

    assertEquals(
        Page.JAVASCRIPT,
        new URLSearchParams(window.location.search).get('page'));

    const jsTab = shadowRoot.querySelector<HTMLElement>(
        `div[slot="tab"][data-page-name="${Page.JAVASCRIPT}"]`)!;
    const allTabs = Array.from(shadowRoot.querySelectorAll('[slot="tab"]'));
    const expectedIndex = allTabs.indexOf(jsTab).toString();
    assertEquals(expectedIndex, tabContainer.getAttribute('selected-index'));
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

// Test the <value-display> and <expandable-json-viewer> elements.
suite('ValueDisplayElementTest', function() {
  let v: Value;
  let valueElement: ValueDisplayElement;

  suiteSetup(async function() {
    await customElements.whenDefined('value-display');
    await customElements.whenDefined('expandable-json-viewer');
  });

  setup(function() {
    v = {} as Value;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    valueElement = document.createElement('value-display');
    document.body.appendChild(valueElement);
  });

  const assertType = (s: string) => {
    const span = valueElement.shadowRoot!.querySelector('#type');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const assertValue = (s: string) => {
    const span = valueElement.shadowRoot!.querySelector('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, s);
  };

  const assertJsonValue = (s: string) => {
    const jsonContainer = getExpandableJsonViewerElementOrFail();
    const jsonValueElement =
        jsonContainer.shadowRoot!.querySelector('#json-value');
    assertTrue(!!jsonValueElement);
    assertEquals(jsonValueElement.textContent, s);
  };

  const getExpandableJsonViewerElementOrFail =
      (): ExpandableJsonViewerElement => {
        const span = valueElement.shadowRoot!.querySelector('#value');
        assertTrue(!!span);
        const jsonContainer = span.querySelector('expandable-json-viewer');
        assertTrue(!!jsonContainer);
        return jsonContainer;
      };

  test('null', () => {
    v.nullValue = 1;
    valueElement.configure(v);
    const span = valueElement.shadowRoot!.querySelector('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'null');
    assertTrue(span.classList.contains('none'));
    assertType('');
  });

  test('trueBool', () => {
    v.boolValue = true;
    valueElement.configure(v);
    const span = valueElement.shadowRoot!.querySelector('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'true');
    assertTrue(span.classList.contains('bool-true'));
    assertType('');
  });

  test('falseBool', () => {
    v.boolValue = false;
    valueElement.configure(v);
    const span = valueElement.shadowRoot!.querySelector('#value');
    assertTrue(!!span);
    assertEquals(span.textContent, 'false');
    assertTrue(span.classList.contains('bool-false'));
    assertType('');
  });

  test('int', () => {
    v.intValue = 1234;
    valueElement.configure(v);
    assertValue('1234');
    assertType('(int)');
  });

  test('string', () => {
    v.stringValue = 'some-string';
    valueElement.configure(v);
    assertValue('some-string');
    assertType('(string)');
  });

  test('listOfInts', () => {
    v.listValue = {} as ListValue;
    v.listValue.storage = [1, 2, 3, 4].map((x) => {
      const v: Value = {} as Value;
      v.intValue = x;
      return v;
    });

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        [
          {'intValue': 1},
          {'intValue': 2},
          {'intValue': 3},
          {'intValue': 4},
        ],
        null, 2));
  });

  test('dictionaryOfInts', () => {
    v.dictionaryValue = {} as DictionaryValue;
    const v1: Value = {} as Value;
    v1.intValue = 10;
    const v2: Value = {} as Value;
    v2.intValue = 20;
    v.dictionaryValue.storage = {'v1': v1, 'v2': v2};

    valueElement.configure(v);
    assertJsonValue(JSON.stringify(
        {
          'v1': {'intValue': 10},
          'v2': {'intValue': 20},
        },
        null, 2));
  });

  test('binary', () => {
    v.binaryValue = [10, 20, 30, 40];
    valueElement.configure(v);
    assertValue('[10,20,30,40]');
    assertType('(binary)');
  });
});

// Test the <expandable-json-viewer> element.
suite('ExpandableJsonViewerElement', function() {
  let jsonViewer: ExpandableJsonViewerElement;
  const kJsonViewerTitle = 'JSON Viewer Title';
  const kJsonContent = '{}';

  suiteSetup(async function() {
    await customElements.whenDefined('expandable-json-viewer');
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    jsonViewer = document.createElement('expandable-json-viewer');
    document.body.appendChild(jsonViewer);
    const preElement = document.createElement('pre');
    preElement.innerText = kJsonContent;
    jsonViewer.configure(preElement, kJsonViewerTitle);
  });

  const getChildElementByIdOrFail = (id: string) => {
    const elem = jsonViewer.shadowRoot!.querySelector(`#${id}`);
    assertTrue(!!elem);
    return elem;
  };

  test('rendersPassedChildElement', () => {
    const preElementFromDOM =
        jsonViewer.shadowRoot!.querySelector('#json-content > pre');
    assertTrue(!!preElementFromDOM);
    assertEquals(preElementFromDOM.textContent, kJsonContent);
  });

  test('clickingJsonHeaderTogglesState', async () => {
    const jsonHeaderElement =
        jsonViewer.shadowRoot!.querySelector<HTMLElement>('#json-header')!;

    assertFalse(jsonViewer.hasAttribute('expanded'));
    jsonHeaderElement.click();
    await microtasksFinished();
    assertTrue(jsonViewer.hasAttribute('expanded'));
    jsonHeaderElement.click();
    await microtasksFinished();
    assertFalse(jsonViewer.hasAttribute('expanded'));
  });

  test('rendersTitleInJsonHeader', () => {
    assertEquals(jsonViewer.getTitleTextForTesting(), kJsonViewerTitle);
  });

  test('clickingJsonHeaderSwitchesIcons', async () => {
    const jsonHeaderElement =
        jsonViewer.shadowRoot!.querySelector<HTMLElement>('#json-header')!;
    const openIcon = getChildElementByIdOrFail('open-icon');
    const closeIcon = getChildElementByIdOrFail('close-icon');

    // Only shows open icon by default
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'), 'none');

    // Check that only close-icon is visible when content is expanded
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'none');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'),
        'block');

    // Only open-icon is visible when collapsed after being expanded
    jsonHeaderElement.click();
    await microtasksFinished();
    assertEquals(
        window.getComputedStyle(openIcon).getPropertyValue('display'), 'block');
    assertEquals(
        window.getComputedStyle(closeIcon).getPropertyValue('display'), 'none');
  });
});
