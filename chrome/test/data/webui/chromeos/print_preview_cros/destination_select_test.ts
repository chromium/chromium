// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select.js';

import {DestinationSelectElement} from 'chrome://os-print/js/destination_select.js';
import {DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationSelect', () => {
  let element: DestinationSelectElement;
  let controller: DestinationSelectController;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement(DestinationSelectElement.is) as
        DestinationSelectElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
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
});
