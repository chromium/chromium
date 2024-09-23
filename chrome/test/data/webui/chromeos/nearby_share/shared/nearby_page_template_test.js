// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://nearby/shared/nearby_page_template.js';

import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {isChildVisible} from '../../test_util.js';

suite('nearby-page-template', function() {
  /** @type {!NearbyPageTemplateElement} */
  let element;

  setup(function() {
    document.body.innerHTML = trustedTypes.emptyHTML;

    element = /** @type {!NearbyPageTemplateElement} */ (
        document.createElement('nearby-page-template'));

    document.body.appendChild(element);
  });

  /**
   * @param {string} selector
   * @return {boolean} Returns true if the element is visible in the shadow dom.
   */
  function isVisible(selector) {
    return isChildVisible(element, selector, false);
  }

  test('No buttons shown by default', async function() {
    assertEquals(
        '', element.shadowRoot.querySelector('#pageTitle').innerHTML.trim());
    assertEquals(
        '', element.shadowRoot.querySelector('#pageSubTitle').innerHTML.trim());
    assertFalse(isVisible('#utilityButton'));
    assertFalse(isVisible('#actionButton'));
    assertFalse(isVisible('#cancelButton'));
    assertFalse(isVisible('#closeButton'));
  });

  test('Everything on', async function() {
    element.title = 'title';
    element.subTitle = 'subTitle';
    element.utilityButtonLabel = 'utility';
    element.cancelButtonLabel = 'cancel';
    element.actionButtonLabel = 'action';

    await waitAfterNextRender(element);

    assertEquals(
        'title',
        element.shadowRoot.querySelector('#pageTitle').innerHTML.trim());
    assertEquals(
        'subTitle',
        element.shadowRoot.querySelector('#pageSubTitle').innerHTML.trim());
    assertTrue(isVisible('#utilityButton'));
    assertTrue(isVisible('#actionButton'));
    assertTrue(isVisible('#cancelButton'));
    assertFalse(isVisible('#closeButton'));

    /** @type {boolean} */
    let utilityTriggered = false;
    element.addEventListener(
        element.utilityButtonEventName, () => utilityTriggered = true);
    element.shadowRoot.querySelector('#utilityButton').click();
    assertTrue(utilityTriggered);

    /** @type {boolean} */
    let cancelTriggered = false;
    // Nearby Share app always expects |event.detail| to be defined
    element.addEventListener(element.cancelButtonEventName, event => {
      cancelTriggered = true;
      assertTrue(!!event.detail);
    });
    element.shadowRoot.querySelector('#cancelButton').click();
    assertTrue(cancelTriggered);

    /** @type {boolean} */
    let actionTrigger = false;
    element.addEventListener(
        element.actionButtonEventName, () => actionTrigger = true);
    element.shadowRoot.querySelector('#actionButton').click();
    assertTrue(actionTrigger);
  });

  test('Close only', async function() {
    element.title = 'title';
    element.subTitle = 'subTitle';
    element.utilityButtonLabel = 'utility';
    element.cancelButtonLabel = 'cancel';
    element.actionButtonLabel = 'action';
    element.closeOnly = true;

    await waitAfterNextRender(element);

    assertEquals(
        'title',
        element.shadowRoot.querySelector('#pageTitle').innerHTML.trim());
    assertEquals(
        'subTitle',
        element.shadowRoot.querySelector('#pageSubTitle').innerHTML.trim());
    assertFalse(isVisible('#utilityButton'));
    assertFalse(isVisible('#actionButton'));
    assertFalse(isVisible('#cancelButton'));
    assertTrue(isVisible('#closeButton'));

    /** @type {boolean} */
    let closeTrigger = false;
    element.addEventListener('close', () => closeTrigger = true);
    element.shadowRoot.querySelector('#closeButton').click();
    assertTrue(closeTrigger);
  });

  test('Open-in-new icon', async function() {
    element.title = 'title';
    element.subTitle = 'subTitle';
    element.utilityButtonLabel = 'utility';

    // Open-in-new icon not shown by default.
    await waitAfterNextRender(element);
    assertFalse(
        !!element.shadowRoot.querySelector('#utilityButton #openInNewIcon'));

    element.utilityButtonOpenInNew = true;
    await waitAfterNextRender(element);
    assertTrue(
        !!element.shadowRoot.querySelector('#utilityButton #openInNewIcon'));
    assertEquals(
        'cr:open-in-new',
        element.shadowRoot.querySelector('#utilityButton #openInNewIcon')
            .getAttribute('icon'));
  });
});
