// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';

import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {createInputKeyParts, KeyInputState, MetaKey, Modifier, ShortcutLabelProperties} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {AcceleratorKeyState} from 'chrome://resources/mojo/ui/base/accelerators/mojom/accelerator.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ShortcutUtils', function() {
  test('createInputKeyParts', async () => {
    const inputAcceleratorProperties: ShortcutLabelProperties = {
      keyDisplay: stringToMojoString16('m'),
      accelerator: {
        modifiers: Modifier.CONTROL,
        keyCode: VKey.kKeyM,
        keyState: AcceleratorKeyState.PRESSED,
        timeStamp: {
          internalValue: 0n,
        },
      },
      originalAccelerator: null,
      shortcutLabelText: getTrustedHTML`<a>test string</a>` as TrustedHTML,
      metaKey: MetaKey.kLauncher,
    };

    const inputKeyParts = createInputKeyParts(inputAcceleratorProperties);
    assertEquals(inputKeyParts.length, 2);
    assertEquals(inputKeyParts[0]!.key, 'ctrl');
    assertEquals(inputKeyParts[0]!.keyState, KeyInputState.MODIFIER_SELECTED);
    assertEquals(inputKeyParts[0]!.metaKey, MetaKey.kLauncher);
    assertEquals(inputKeyParts[1]!.key, 'm');
    assertEquals(
        inputKeyParts[1]!.keyState, KeyInputState.ALPHANUMERIC_SELECTED);
  });
});
