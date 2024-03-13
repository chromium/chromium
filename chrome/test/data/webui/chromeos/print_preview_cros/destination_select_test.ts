// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select.js';

import {DestinationDropdownElement} from 'chrome://os-print/js/destination_dropdown.js';
import {DestinationSelectElement} from 'chrome://os-print/js/destination_select.js';
import {DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationSelect', () => {
  let element: DestinationSelectElement;
  let controller: DestinationSelectController;
  let mockController: MockController;

  const loadingSelector = '#loading';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockController = new MockController();

    element = document.createElement(DestinationSelectElement.is) as
        DestinationSelectElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
    mockController.reset();
  });

  // Verify the print-preview-cros-app element can be rendered.
  test('element renders', () => {
    assertTrue(
        isVisible(element), `Should display ${DestinationSelectElement.is}`);
  });

  // Verify destination-select element has a controller configured.
  test('has element controller', () => {
    assertTrue(
        !!controller,
        `${DestinationSelectElement.is} should have controller configured`);
  });

  // Verify expected elements display while `controller.shouldShowLoading` is
  // true.
  test('displays expected elements when showLoading is true', () => {
    const isLoadingFn =
        mockController.createFunctionMock(controller, 'shouldShowLoading');
    isLoadingFn.returnValue = true;

    // Remove and re-add element to page to trigger 'connectedCallback'.
    element.remove();
    document.body.append(element);

    assertTrue(
        isChildVisible(element, loadingSelector),
        `Loading UX should be visible`);
    assertFalse(
        isChildVisible(element, DestinationDropdownElement.is),
        `${DestinationDropdownElement.is} should not be visible`);
  });

  // Verify expected elements display while `controller.shouldShowLoading` is
  // false.
  test('displays expected loading UX', () => {
    const loadingFn =
        mockController.createFunctionMock(controller, 'shouldShowLoading');
    loadingFn.returnValue = false;

    // Remove and re-add element to page to trigger 'connectedCallback'.
    element.remove();
    document.body.append(element);

    assertFalse(
        isChildVisible(element, loadingSelector),
        `Loading UX should not be visible`);
    assertTrue(
        isChildVisible(element, DestinationDropdownElement.is),
        `${DestinationDropdownElement.is} should be visible`);
  });
});
