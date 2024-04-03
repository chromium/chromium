// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_row_controller.js';

import {DestinationRowElement} from 'chrome://os-print/js/destination_row.js';
import {DestinationRowController} from 'chrome://os-print/js/destination_row_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationRow', () => {
  let element: DestinationRowElement;
  let controller: DestinationRowController;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement(DestinationRowElement.is) as
        DestinationRowElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
  });

  // Verify the element can be rendered.
  test('element renders', () => {
    assertTrue(
        isVisible(element), `Should display ${DestinationRowElement.is}`);
  });

  // Verify the element has a controller configured.
  test('has element controller', () => {
    assertTrue(
        !!controller,
        `${DestinationRowElement.is} should have controller configured`);
  });
});
