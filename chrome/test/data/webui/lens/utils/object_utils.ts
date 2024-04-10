// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {OverlayObject} from 'chrome-untrusted://lens/overlay_object.mojom-webui.js';
import {assertEquals, assertLT} from 'chrome-untrusted://webui-test/chai_assert.js';

export function assertBoxesWithinThreshold(
    box1: CenterRotatedBox, box2: CenterRotatedBox) {
  const threshold: number = 1e-6;

  assertLT(Math.abs(box1.box.x - box2.box.x), threshold);
  assertLT(Math.abs(box1.box.y - box2.box.y), threshold);
  assertLT(Math.abs(box1.box.height - box2.box.height), threshold);
  assertLT(Math.abs(box1.box.width - box2.box.width), threshold);
  assertEquals(box1.rotation, box2.rotation);
  assertEquals(box1.coordinateType, box2.coordinateType);
}

export function createObject(id: string, boundingBox: RectF): OverlayObject {
  return {
    id,
    geometry: {
      boundingBox: {
        box: boundingBox,
        rotation: 0,
        coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      },
    },
  };
}
