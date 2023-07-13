// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/button_label.js';

import {ButtonLabelElement} from 'chrome://customize-chrome-side-panel.top-chrome/button_label.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {assertNotStyle, assertStyle} from './test_support.js';


suite('ButtonLabelTest', () => {
  let buttonLabelElement: ButtonLabelElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    buttonLabelElement =
        document.createElement('customize-chrome-button-label');
    document.body.appendChild(buttonLabelElement);
  });

  test(
      'setting `label` gives the button label element text content',
      async () => {
        // Act.
        buttonLabelElement.label = 'foo';

        // Assert.
        assertEquals('foo', buttonLabelElement.$.label.textContent);
        assertStyle(buttonLabelElement.$.labelDescription, 'display', 'none');
      });

  test(
      'setting `labelDescription` makes the label description show',
      async () => {
        // Act.
        buttonLabelElement.label = 'foo';
        buttonLabelElement.labelDescription = 'bar';

        // Assert.
        assertNotStyle(
            buttonLabelElement.$.labelDescription, 'display', 'none');
        assertEquals('foo', buttonLabelElement.$.label.textContent);
        assertEquals(
            'bar', buttonLabelElement.$.labelDescription.textContent!.trim());
      });
});
