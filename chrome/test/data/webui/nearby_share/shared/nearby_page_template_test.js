// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://nearby/shared/nearby_page_template.m.js';
// #import {waitAfterNextRender, isChildVisible} from '../../test_util.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// clang-format on

suite('nearby-page-template', function() {
  /** @type {!NearbyPageTemplateElement} */
  let element;

  setup(function() {
    document.body.innerHTML = '';

    element = /** @type {!NearbyPageTemplateElement} */ (
        document.createElement('nearby-page-template'));

    document.body.appendChild(element);
  });

  /**
   * @param {string} selector
   * @return {boolean} Returns true if the element is visible in the shadow dom.
   */
  function isVisible(selector) {
    return test_util.isChildVisible(element, selector, false);
  }

  test('No buttons shown by default', async function() {
    assertEquals('', element.$$('#pageTitle').innerHTML.trim());
    assertEquals('', element.$$('#pageSubTitle').innerHTML.trim());
    assertFalse(isVisible('#utilityButton'));
    assertFalse(isVisible('#actionButton'));
    assertFalse(isVisible('#cancelButton'));
  });

  test('Everything on', async function() {
    element.title = 'title';
    element.subTitle = 'subTitle';
    element.utilityButtonLabel = 'utility';
    element.cancelButtonLabel = 'cancel';
    element.actionButtonLabel = 'action';

    await test_util.waitAfterNextRender(element);

    assertEquals('title', element.$$('#pageTitle').innerHTML.trim());
    assertEquals('subTitle', element.$$('#pageSubTitle').innerHTML.trim());
    assertTrue(isVisible('#utilityButton'));
    assertTrue(isVisible('#actionButton'));
    assertTrue(isVisible('#cancelButton'));

    /** @type {boolean} */
    let utilityTriggered = false;
    element.addEventListener(
        element.utilityButtonEventName, () => utilityTriggered = true);
    element.$$('#utilityButton').click();
    assertTrue(utilityTriggered);

    /** @type {boolean} */
    let cancelTriggered = false;
    element.addEventListener(
        element.cancelButtonEventName, () => cancelTriggered = true);
    element.$$('#cancelButton').click();
    assertTrue(cancelTriggered);

    /** @type {boolean} */
    let actionTrigger = false;
    element.addEventListener(
        element.actionButtonEventName, () => actionTrigger = true);
    element.$$('#actionButton').click();
    assertTrue(actionTrigger);
  });
});
