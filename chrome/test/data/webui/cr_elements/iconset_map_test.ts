// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

suite('cr-iconset', function() {
  let iconsetMap: IconsetMap;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function addIronIconsetSvg(name: string) {
    const div = document.createElement('div');
    div.innerHTML = getTrustedHtml(`
      <iron-iconset-svg name="${name}" size="24">
        <svg>
          <defs>
            <g id="arrow-drop-up"></g>
          </defs>
        </svg>
      </iron-iconset-svg>`);
    document.head.appendChild(div.querySelector('iron-iconset-svg')!);
  }

  test('iron-iconset-svg compatibility', () => {
    addIronIconsetSvg('test1');

    // Reset the singleton instance to simulate the case where pre-existing
    // iron-iconset-svg instances already exist in <head>.
    iconsetMap = new IconsetMap();
    IconsetMap.resetInstanceForTesting(iconsetMap);

    // Ensure that pre-existing instances are detected.
    assertTrue(!!iconsetMap.get('test1'));
    assertFalse(!!iconsetMap.get('test2'));

    // Ensure that newly registered instances are also detected.
    addIronIconsetSvg('test2');
    assertTrue(!!iconsetMap.get('test1'));
    assertTrue(!!iconsetMap.get('test2'));
  });
});
