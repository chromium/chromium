// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/utils/validation_utils.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {isValidDestination} from 'chrome://os-print/js/utils/validation_utils.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {createTestDestination, resetDataManagersAndProviders} from './test_utils.js';

suite('ValidationUtils', () => {
  let destinationManager: DestinationManager;

  setup(() => {
    resetDataManagersAndProviders();
    destinationManager = DestinationManager.getInstance();
  });

  teardown(() => resetDataManagersAndProviders());

  // Verify isValidateDestination returns false if:
  // 1. DestinationManager is not initialized.
  // 2. Provided ID does not exist in the DestinationManager.
  test('isValidDestination is false', () => {
    const fakeDestination = createTestDestination();
    destinationManager.setDestinationForTesting(fakeDestination);
    assertFalse(
        isValidDestination(fakeDestination.id),
        'Destination manager not initialized');

    // Initialize session so validation will pass the "is initialized" check.
    destinationManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    assertFalse(
        isValidDestination('unknownDestinationId'),
        'Destination must be in manager');
  });

  // Verify isValidateDestination returns true if destination exists in the
  // DestinationManager and manager has been initialized.
  test('isValidDestination is true', () => {
    const fakeDestination = createTestDestination();
    destinationManager.setDestinationForTesting(fakeDestination);
    destinationManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    assertTrue(
        isValidDestination(PDF_DESTINATION.id),
        'Destination manager has PDF destination');
  });
});
