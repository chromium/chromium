// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/print_preview_cros_app.js';

import {PrintPreviewCrosAppElement} from 'chrome://os-print/js/print_preview_cros_app.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PrintPreviewCrosApp', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  // Verify the print-preview-cros-app element can be rendered.
  test('element renders', () => {
    const element = document.createElement(PrintPreviewCrosAppElement.is) as
        PrintPreviewCrosAppElement;
    assertTrue(!!element);
    document.body.append(element);
    assertTrue(
        isVisible(element), `Should display ${PrintPreviewCrosAppElement.is}`);
  });
});
