// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';

import type {HeaderElement} from 'chrome://compare/header.js';
import {ProductSpecificationsBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, hasStyle, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProductSpecificationsBrowserProxy} from './test_product_specifications_browser_proxy.js';

suite('HeaderTest', () => {
  let header: HeaderElement;
  const productSpecsProxy = new TestProductSpecificationsBrowserProxy();

  setup(async () => {
    productSpecsProxy.reset();
    ProductSpecificationsBrowserProxyImpl.setInstance(productSpecsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    header = document.createElement('product-specifications-header');
    document.body.appendChild(header);
    await microtasksFinished();
  });

  test('menu shown on click', async () => {
    const menu = header.$.menu.$.menu;

    assertEquals(menu.getIfExists(), null);

    header.$.menuButton.click();
    await microtasksFinished();

    assertNotEquals(menu.getIfExists(), null);
  });

  test('button changes background color when menu is showing', async () => {
    const menuButton = header.$.menuButton;
    const baseBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!baseBackgroundColor);
    menuButton.click();
    await microtasksFinished();

    const menuShownBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!menuShownBackgroundColor);
    assertNotEquals(
        menuShownBackgroundColor.toString(), baseBackgroundColor.toString());

    header.$.menu.close();
    menuButton.blur();
    await eventToPromise('close', header.$.menu);

    const menuClosedBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!menuClosedBackgroundColor);
    assertEquals(
        baseBackgroundColor.toString(), menuClosedBackgroundColor.toString());
  });

  test('menu rename shows input', async () => {
    header.$.menuButton.click();
    await microtasksFinished();

    assertFalse(!!header.shadowRoot.querySelector('#input'));
    const menu = header.$.menu.$.menu;
    const renameMenuItem = menu.get().querySelector<HTMLElement>('#rename');
    assertTrue(!!renameMenuItem);
    renameMenuItem.click();
    await microtasksFinished();

    assertTrue(!!header.shadowRoot.querySelector('#input'));
    assertFalse(menu.get().open);
  });

  test('input hides and changes name on enter', async () => {
    header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
    await microtasksFinished();

    const input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    assertTrue(isVisible(input));
    const newName = 'new name';
    input.value = newName;
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    assertFalse(isVisible(input));
    assertEquals(newName, header.subtitle);
  });

  test('setting `subtitle` gives the header a subtitle', async () => {
    const subtitle = $$(header, '#subtitle');
    assertTrue(!!subtitle);
    assertTrue(hasStyle(subtitle, 'display', 'none'));
    assertTrue(hasStyle(header.$.divider, 'display', 'none'));
    assertTrue(hasStyle(header.$.menuButton, 'display', 'none'));

    header.subtitle = 'foo';
    await microtasksFinished();

    assertEquals('foo', subtitle.textContent.trim());
    assertFalse(hasStyle(subtitle, 'display', 'none'));
    assertFalse(hasStyle(header.$.divider, 'display', 'none'));
    assertFalse(hasStyle(header.$.menuButton, 'display', 'none'));
  });

  test('subtitle is editable on enter', async () => {
    const subtitle = $$(header, '#subtitle');
    assertTrue(!!subtitle);
    subtitle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    const input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    assertTrue(isVisible(input));
  });

  test('subtitle and input do not change after empty input', async () => {
    const subtitle = 'foo';

    header.subtitle = subtitle;
    header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
    await microtasksFinished();

    let input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    input.value = '';
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    // After finishing input, the element should be removed from the DOM.
    input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertFalse(!!input);

    assertEquals(subtitle, header.subtitle);
  });

  test('cursor moves back to the end of the input on blur', async () => {
    header.subtitle = 'foo bar baz';
    header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
    await microtasksFinished();

    // Select a middle section of the input text.
    let input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    input.select(5, 9);
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    // After finishing input, the element should be removed from the DOM.
    input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertFalse(!!input);
  });

  test('`name-change` event is fired on input blur', async () => {
    const subtitle = $$(header, '#subtitle');
    assertTrue(!!subtitle);
    subtitle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    const input = header.shadowRoot.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    assertTrue(isVisible(input));
    const nameChangePromise = eventToPromise('name-change', document.body);
    input.value = 'foo';
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    const event = await nameChangePromise;

    // Ensure the event contains the new name.
    assertTrue(!!event);
    assertTrue(!!event.detail);
    assertTrue(event.detail.name === 'foo');
  });

  test(
      'page title click redirects to compare page when clickable', async () => {
        header.isPageTitleClickable = false;
        await microtasksFinished();

        const pageTitle = $$(header, '#title');
        assertTrue(!!pageTitle);

        // Nothing should happen while the title is not clickable.
        pageTitle.click();
        await microtasksFinished();
        assertEquals(0, productSpecsProxy.getCallCount('showComparePage'));

        header.isPageTitleClickable = true;
        await microtasksFinished();

        // Compare page should open in the same tab.
        pageTitle.click();
        await microtasksFinished();
        assertEquals(1, productSpecsProxy.getCallCount('showComparePage'));
        assertEquals(
            /*inNewTab=*/ false,
            productSpecsProxy.getArgs('showComparePage')[0]);
      });

  test(
      'menu button and subtitle input are unavailable when disabled',
      async () => {
        header.disabled = true;
        await microtasksFinished();

        // Menu button is disabled.
        assertTrue(header.$.menuButton.disabled);

        const subtitle = $$(header, '#subtitle');
        assertTrue(!!subtitle);
        subtitle.click();
        await microtasksFinished();

        // Header input does not appear when disabled.
        const input = header.shadowRoot.querySelector<CrInputElement>('#input');
        assertFalse(!!input);
      });
});
