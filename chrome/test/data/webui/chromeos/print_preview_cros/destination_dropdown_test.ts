// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown.js';

import {DestinationDropdownElement} from 'chrome://os-print/js/destination_dropdown.js';
import {DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationDropdown', () => {
  let element: DestinationDropdownElement;
  let controller: DestinationDropdownController;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement(DestinationDropdownElement.is) as
        DestinationDropdownElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
  });

  // Verify the dropdown can be added to UI.
  test('element renders', () => {
    assertTrue(
        isVisible(element), `Should display ${DestinationDropdownElement.is}`);
  });

  // Verify dropdown element has a controller configured.
  test('has element controller', async () => {
    assertTrue(
        !!controller,
        `${DestinationDropdownElement.is} should have controller configured`);
  });
});
