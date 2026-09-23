// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {doElementsOverlap} from 'chrome://search-engine-choice/app_refresh.js';
import type {AppRefreshElement} from 'chrome://search-engine-choice/app_refresh.js';
import type {SearchEngineChoice} from 'chrome://search-engine-choice/search_engine_choice.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://search-engine-choice/search_engine_choice.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

// Declared before the suites below that instantiate the element, so that these
// pure geometry tests never run with a leftover element attached to the DOM.
suite('DoElementsOverlap', function() {
  test('Overlap', function() {
    // Overlapping rectangles.
    const rect1 = new DOMRect(0, 0, 100, 100);
    const rect2 = new DOMRect(50, 50, 100, 100);
    assertTrue(doElementsOverlap(rect1, rect2));
    assertTrue(doElementsOverlap(rect2, rect1));
  });

  test('VerticalSeparation', function() {
    // rect1 is strictly above rect2.
    const rect1 = new DOMRect(0, 0, 100, 50);
    const rect2 = new DOMRect(0, 60, 100, 50);
    assertFalse(doElementsOverlap(rect1, rect2));
    assertFalse(doElementsOverlap(rect2, rect1));
  });

  test('HorizontalSeparation', function() {
    // rect1 is to the left of rect2 with no offset.
    const rect1 = new DOMRect(0, 0, 50, 100);
    const rect2 = new DOMRect(60, 0, 50, 100);
    assertFalse(doElementsOverlap(rect1, rect2));
    assertFalse(doElementsOverlap(rect2, rect1));
  });

  test('HorizontalOffset', function() {
    // rect1 and rect2 are separated horizontally by 10px (50 to 60).
    const rect1 = new DOMRect(0, 0, 50, 100);
    const rect2 = new DOMRect(60, 0, 50, 100);

    // With offset = 30, overlap is detected.
    assertTrue(doElementsOverlap(rect1, rect2, 30));
    assertTrue(doElementsOverlap(rect2, rect1, 30));

    // rect3 is separated by 40px (50 to 90), which exceeds offset = 30.
    const rect3 = new DOMRect(90, 0, 50, 100);
    assertFalse(doElementsOverlap(rect1, rect3, 30));
    assertFalse(doElementsOverlap(rect3, rect1, 30));
  });
});

function createMockChoiceList(): SearchEngineChoice[] {
  return [
    {
      prepopulateId: 1,
      name: 'Google',
      iconPath: 'images/google.png',
      url: 'https://google.com',
      marketingSnippet: 'Google Search',
      showMarketingSnippet: false,
    },
    {
      prepopulateId: 2,
      name: 'Bing',
      iconPath: 'images/bing.png',
      url: 'https://bing.com',
      marketingSnippet: 'Bing Search',
      showMarketingSnippet: false,
    },
  ];
}

function getMockLoadTimeDataValues(): Record<string, string|boolean> {
  return {
    choiceList: JSON.stringify(createMockChoiceList()),
    choiceListA11yLabel: 'Choices',
    guestCheckboxText: 'Guest Mode',
    infoDialogButtonText: 'Close',
    infoDialogFirstParagraph: 'P1',
    infoDialogSecondParagraph: 'P2',
    infoDialogThirdParagraph: 'P3',
    infoDialogTitle: 'Dialog Title',
    moreButtonText: 'More',
    productLogoAltText: 'Logo',
    showGuestCheckbox: false,
    submitButtonText: 'Submit',
    subtitle: 'Subtitle',
    subtitleInfoLink: 'InfoLink',
    subtitleInfoLinkA11yLabel: 'InfoLinkA11y',
    title: 'Title',
  };
}

suite('SearchEngineChoiceRefreshTest', function() {
  let app: AppRefreshElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(function() {
    loadTimeData.overrideValues(getMockLoadTimeDataValues());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    browserProxyFactory.setInstance({handler});

    app = document.createElement('search-engine-choice-app-refresh');
    document.body.appendChild(app);
    return microtasksFinished();
  });

  test('Initialization', function() {
    assertEquals(1, handler.getCallCount('displayDialog'));
    const radioButtons = app.shadowRoot.querySelectorAll('cr-radio-button');
    assertEquals(2, radioButtons.length);
    const radio1 = radioButtons[0];
    assertTrue(!!radio1);
    assertEquals('1', radio1.name);
    const radio2 = radioButtons[1];
    assertTrue(!!radio2);
    assertEquals('2', radio2.name);
    // It's hidden since loadTimeData set `showGuestCheckbox` to false.
    assertTrue(app.$.guestCheckbox.hidden);
    assertFalse(isVisible(app.$.guestCheckbox));
    assertFalse(app.$.choiceList.classList.contains('overlap-mitigation'));
    assertFalse(app.$.buttonContainer.classList.contains('overlap-mitigation'));
  });

  test('LearnMoreLinkClicked', async function() {
    app.$.infoLink.click();
    await handler.whenCalled('handleLearnMoreLinkClicked');

    const infoDialog = app.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!infoDialog);
    assertTrue(infoDialog.open);

    const infoDialogButton =
        app.shadowRoot.querySelector<HTMLElement>('#infoDialogButton');
    assertTrue(!!infoDialogButton);
    infoDialogButton.click();
    await microtasksFinished();

    assertFalse(!!app.shadowRoot.querySelector('cr-dialog'));
  });

  test('ChoiceSelectionAndSubmit', async function() {
    assertTrue(app.$.actionButton.disabled);

    const radioButtons =
        app.shadowRoot.querySelectorAll<HTMLElement>('cr-radio-button');
    const radio1 = radioButtons[0];
    assertTrue(!!radio1);
    radio1.click();
    await microtasksFinished();

    assertFalse(app.$.actionButton.disabled);
    app.$.actionButton.click();

    const [prepopulateId, keepBrowsingData] =
        await handler.whenCalled('handleSearchEngineChoiceSelected');

    assertEquals(createMockChoiceList()[0]!.prepopulateId, prepopulateId);
    assertFalse(keepBrowsingData);
  });
});

suite('SearchEngineChoiceRefreshTestGuestCheckbox', function() {
  let app: AppRefreshElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(function() {
    loadTimeData.overrideValues(Object.assign(getMockLoadTimeDataValues(), {
      showGuestCheckbox: true,
    }));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    browserProxyFactory.setInstance({handler});

    app = document.createElement('search-engine-choice-app-refresh');
    document.body.appendChild(app);
    return microtasksFinished();
  });

  test('GuestCheckboxVisibilityAndSubmit', async function() {
    const guestCheckbox = app.$.guestCheckbox;
    // It's visible since loadTimeData set `showGuestCheckbox` to true.
    assertFalse(guestCheckbox.hidden);
    assertTrue(isVisible(guestCheckbox));
    assertFalse(app.$.choiceList.classList.contains('overlap-mitigation'));
    assertFalse(app.$.buttonContainer.classList.contains('overlap-mitigation'));

    const radioButtons =
        app.shadowRoot.querySelectorAll<HTMLElement>('cr-radio-button');
    const radio1 = radioButtons[0];
    assertTrue(!!radio1);
    radio1.click();
    await microtasksFinished();

    guestCheckbox.click();
    await microtasksFinished();

    app.$.actionButton.click();

    const [prepopulateId, keepBrowsingData] =
        await handler.whenCalled('handleSearchEngineChoiceSelected');
    assertEquals(createMockChoiceList()[0]!.prepopulateId, prepopulateId);
    assertTrue(keepBrowsingData);
  });
});
