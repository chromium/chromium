// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_mac.h"

#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace viz {

namespace {

TEST(SoftwareOutputDeviceMacTest, Basics) {
  auto device = std::make_unique<SoftwareOutputDeviceMac>(
      base::SequencedTaskRunner::GetCurrentDefault());
  gfx::Size pixel_size(512, 512);
  float scale_factor = 1;

  EXPECT_EQ(device->BufferQueueSizeForTesting(), 0u);
  device->Resize(pixel_size, scale_factor);

  // Frame 0.
  gfx::Rect damage0(pixel_size);
  device->BeginPaint(damage0);
  IOSurfaceRef io_surface0 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();

  // Frame 1.
  // We didn't set the IOSurface in use, so it should be re-used, and we should
  // have no copy.
  gfx::Rect damage1(10, 10, 10, 10);
  device->BeginPaint(damage1);
  IOSurfaceRef io_surface1 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();
  EXPECT_EQ(io_surface0, io_surface1);
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 1u);
  EXPECT_TRUE(device->LastCopyRegionForTesting().isEmpty());

  // Frame 2.
  // The IOSurface is in use, so we should allocate a new one. We'll do a full
  // copy because it's a new buffer.
  IOSurfaceIncrementUseCount(io_surface1);
  gfx::Rect damage2(20, 20, 10, 10);
  device->BeginPaint(damage2);
  IOSurfaceRef io_surface2 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();
  EXPECT_NE(io_surface1, io_surface2);
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 2u);
  SkRegion copy_region2(gfx::RectToSkIRect(gfx::Rect(pixel_size)));
  copy_region2.op(gfx::RectToSkIRect(damage2), SkRegion::kDifference_Op);
  EXPECT_EQ(device->LastCopyRegionForTesting(), copy_region2);

  // Frame 3.
  // Both IOSurfaces are in use, so we'll allocate yet a new one and do another
  // full copy.
  IOSurfaceIncrementUseCount(io_surface2);
  gfx::Rect damage3(30, 30, 10, 10);
  device->BeginPaint(damage3);
  IOSurfaceRef io_surface3 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();
  EXPECT_NE(io_surface1, io_surface3);
  EXPECT_NE(io_surface2, io_surface3);
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 3u);
  SkRegion copy_region3(gfx::RectToSkIRect(gfx::Rect(pixel_size)));
  copy_region3.op(gfx::RectToSkIRect(damage3), SkRegion::kDifference_Op);
  EXPECT_EQ(device->LastCopyRegionForTesting(), copy_region3);

  // Frame 4.
  // The IOSurface from frame1 is free, so we should re-use it. We should be
  // copying the damage from frame2 and frame3.
  IOSurfaceIncrementUseCount(io_surface3);
  IOSurfaceDecrementUseCount(io_surface1);
  gfx::Rect damage4(35, 35, 15, 15);
  device->BeginPaint(damage4);
  IOSurfaceRef io_surface4 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();
  EXPECT_EQ(io_surface1, io_surface4);
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 3u);
  SkRegion copy_region4;
  copy_region4.op(gfx::RectToSkIRect(damage2), SkRegion::kUnion_Op);
  copy_region4.op(gfx::RectToSkIRect(damage3), SkRegion::kUnion_Op);
  copy_region4.op(gfx::RectToSkIRect(damage4), SkRegion::kDifference_Op);
  EXPECT_EQ(device->LastCopyRegionForTesting(), copy_region4);

  // Frame 5.
  // All IOSurfaces are allocated, allocate another.
  IOSurfaceIncrementUseCount(io_surface4);
  gfx::Rect damage5(50, 50, 10, 10);
  device->BeginPaint(damage5);
  IOSurfaceRef io_surface5 = device->CurrentPaintIOSurfaceForTesting();
  EXPECT_NE(io_surface5, io_surface2);
  EXPECT_NE(io_surface5, io_surface3);
  EXPECT_NE(io_surface5, io_surface4);
  device->EndPaint();
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 4u);

  // Frame 6.
  // All IOSurfaces are in use, allocate another, but free the one from frame2
  // (add an extra retain and check to retain count to verify that it steps
  // down).
  IOSurfaceIncrementUseCount(io_surface5);
  CFRetain(io_surface2);
  EXPECT_EQ(CFGetRetainCount(io_surface2), 2u);
  gfx::Rect damage6(60, 60, 10, 10);
  device->BeginPaint(damage6);
  IOSurfaceRef io_surface6 = device->CurrentPaintIOSurfaceForTesting();
  device->EndPaint();
  EXPECT_EQ(device->BufferQueueSizeForTesting(), 4u);
  EXPECT_NE(io_surface6, io_surface2);
  EXPECT_NE(io_surface6, io_surface3);
  EXPECT_NE(io_surface6, io_surface4);
  EXPECT_NE(io_surface6, io_surface5);
  EXPECT_EQ(CFGetRetainCount(io_surface2), 1u);
  CFRelease(io_surface2);
}

}  // namespace

}  // namespace viz
