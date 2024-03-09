// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/destination_manager.js';

import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('DestinationManager', () => {
  setup(() => {
    DestinationManager.resetInstanceForTesting();
  });

  teardown(() => {
    DestinationManager.resetInstanceForTesting();
  });

  test('is a singleton', () => {
    const instance1 = DestinationManager.getInstance();
    const instance2 = DestinationManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = DestinationManager.getInstance();
    DestinationManager.resetInstanceForTesting();
    const instance2 = DestinationManager.getInstance();
    assertTrue(instance1 !== instance2);
  });

  // Verify `hasInitialDestinationsLoaded` returns false by default.
  test('on create hasInitialDestinationsLoaded is false', () => {
    const instance = DestinationManager.getInstance();
    assertFalse(instance.hasInitialDestinationsLoaded());
  });
});
