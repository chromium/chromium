// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/shared/nearby_preview.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ShareType} from 'chrome://nearby/shared/nearby_share_share_type.mojom-webui.js';

import {assertEquals} from '../../chai_assert.js';

suite('PreviewTest', function() {
  /** @type {!NearbyPreviewElement} */
  let previewElement;

  setup(function() {
    previewElement = /** @type {!NearbyPreviewElement} */ (
        document.createElement('nearby-preview'));
    document.body.appendChild(previewElement);
  });

  teardown(function() {
    previewElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-PREVIEW', previewElement.tagName);
  });

  test('renders title', function() {
    const title = 'Title';
    previewElement.payloadPreview = {
      description: title,
      fileCount: 1,
      shareType: /** @type {ShareType} */ (0),
    };

    const renderedTitle =
        previewElement.shadowRoot.querySelector('#title').textContent;
    assertEquals(title, renderedTitle);
  });
});
