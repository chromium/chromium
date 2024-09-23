// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_row_controller.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DestinationRowElement} from 'chrome://os-print/js/destination_row.js';
import {DestinationRowController} from 'chrome://os-print/js/destination_row_controller.js';
import {Destination} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders} from './test_utils.js';

suite('DestinationRow', () => {
  let element: DestinationRowElement;
  let controller: DestinationRowController;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    resetDataManagersAndProviders();
    element = document.createElement(DestinationRowElement.is) as
        DestinationRowElement;
    element.destination = PDF_DESTINATION;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
    resetDataManagersAndProviders();
  });

  function getTextContent(selector: string): string {
    assert(element);
    const textElement: HTMLElement =
        strictQuery<HTMLElement>(selector, element.shadowRoot, HTMLElement);
    assert(textElement.textContent);
    return textElement.textContent!.trim();
  }

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

  // Verify destination display name displayed.
  test('label matches provided destination display name', () => {
    const labelSelector = '#label';
    assertEquals(
        PDF_DESTINATION.displayName, getTextContent(labelSelector),
        'PDF display name should be shown');

    // Change destination to verify UI matches updated destination.
    const destination: Destination = createTestDestination();
    element.destination = destination;

    assertEquals(
        destination.displayName, getTextContent(labelSelector),
        'Fake destination display name should be shown');
  });
});
