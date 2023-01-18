// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShareType} from 'chrome://nearby/mojo/nearby_share_share_type.mojom-webui.js';
import {NearbyPreviewElement} from 'chrome://nearby/shared/nearby_preview.js';

import {assertEquals} from '../../chromeos/chai_assert.js';

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
