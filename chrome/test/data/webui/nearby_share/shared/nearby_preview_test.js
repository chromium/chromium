// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// So that mojo is defined.
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share_target_types.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share_share_type.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share.mojom-lite.js';
// #import 'chrome://nearby/shared/nearby_preview.m.js';
// #import {assertEquals} from '../../chai_assert.js';
// clang-format on

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
      shareType: /** @type {nearbyShare.mojom.ShareType} */ (0)
    };

    const renderedTitle = previewElement.$$('#title').textContent;
    assertEquals(title, renderedTitle);
  });
});
