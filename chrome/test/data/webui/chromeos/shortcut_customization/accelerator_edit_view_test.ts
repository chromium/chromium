// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {AcceleratorSource, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorEditViewTest', function() {
  let editViewElement: AcceleratorEditViewElement|null = null;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);

    editViewElement = document.createElement('accelerator-edit-view');
    document.body.appendChild(editViewElement);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    editViewElement!.remove();
    editViewElement = null;
  });

  function getElementById(id: string): HTMLElement {
    assertTrue(!!editViewElement);
    const element = editViewElement.shadowRoot!.getElementById(id);
    assertTrue(!!element);
    return element as HTMLElement;
  }

  test('LoadsBasicEditView', async () => {
    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    await flush();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));


    // Click on the edit button.
    getElementById('editButton')!.click();

    // Only the Cancel button should now be visible.
    assertFalse(isVisible(getElementById('editButtonsContainer')));
    assertTrue(isVisible(getElementById('cancelButtonContainer')));


    // Click on the Cancel button and expect the edit buttons to be available.
    getElementById('cancelButton')!.click();
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));
  });

  test('DetectShortcutConflict', async () => {
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    editViewElement!.acceleratorInfo = acceleratorInfo;
    editViewElement!.source = AcceleratorSource.kAsh;
    editViewElement!.action = 1;
    await flushTasks();

    // Check that the edit buttons are visible.
    assertTrue(isVisible(getElementById('editButtonsContainer')));
    assertFalse(isVisible(getElementById('cancelButtonContainer')));

    // Assert that no error has occurred.
    assertFalse(editViewElement!.hasError);

    // Click on the edit button.
    getElementById('editButton')!.click();

    // Press 'Snap Window left' key, expect an error due since it is a
    // pre-existing shortcut.
    const viewElement = getElementById('acceleratorItem');
    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: '[',
      keyCode: 219,
      code: 'Key[',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();
    assertTrue(editViewElement!.hasError);

    // Press another shortcut, expect no error.
    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'e',
      keyCode: 69,
      code: 'KeyE',
      ctrlKey: true,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flushTasks();
    assertFalse(editViewElement!.hasError);
  });
});
