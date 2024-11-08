// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://os-settings/os_settings.js';

import {OsSettingsSubpageElement, SettingsCardElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<os-settings-subpage>', () => {
  suite('multiCard property', () => {
    let subpage: OsSettingsSubpageElement;

    teardown(() => {
      subpage.remove();
    });

    function init(multiCard: boolean = false): void {
      subpage = document.createElement('os-settings-subpage');
      subpage.multiCard = multiCard;
      document.body.appendChild(subpage);

      flush();
    }

    function getCard(): SettingsCardElement|null {
      return subpage.shadowRoot!.querySelector<SettingsCardElement>(
          '#cardBody');
    }

    test('if false, subpage will populate the html with a card', () => {
      init();

      const subpageCard = getCard()
      assertTrue(!!subpageCard);
    });

    test('if true, subpage will not populate itself with a card', () => {
      init(/*multiCard=*/ true);

      const subpageCard = getCard();
      assertFalse(!!subpageCard);
    });
  });
});
