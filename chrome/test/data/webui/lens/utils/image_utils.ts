// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

export function waitForScreenshotRendered(selectionOverlay: HTMLElement):
    Promise<void> {
  return new Promise<void>((resolve) => {
    const observer = new ResizeObserver(() => {
      if (selectionOverlay.offsetHeight > 0) {
        resolve();
        observer.unobserve(selectionOverlay);
      }
    });
    observer.observe(selectionOverlay);
  });
}

export function fakeScreenshotBitmap(
    width = 1, height = 1): BitmapMappedFromTrustedProcess {
  return {
    imageInfo: {
      width,
      height,
      colorType: 0,
      alphaType: 0,
      colorTransferFunction: [],
      colorToXyzMatrix: [],
    },
    pixelData: {
      bytes: Array(4 * width * height).fill(0),
    } as BigBuffer,
    uNUSEDRowBytes: BigInt(0),
  };
}
