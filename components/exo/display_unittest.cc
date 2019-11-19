// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/wm/desks/desks_util.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/data_device.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/file_helper.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace exo {
namespace {

using DisplayTest = test::ExoTestBase;

TEST_F(DisplayTest, CreateSurface) {
  std::unique_ptr<Display> display(new Display);

  // Creating a surface should succeed.
  std::unique_ptr<Surface> surface = display->CreateSurface();
  EXPECT_TRUE(surface);
}

TEST_F(DisplayTest, CreateSharedMemory) {
  std::unique_ptr<Display> display(new Display);

  int shm_size = 8192;
  base::UnsafeSharedMemoryRegion shared_memory =
      base::UnsafeSharedMemoryRegion::Create(shm_size);
  ASSERT_TRUE(shared_memory.IsValid());

  // Creating a shared memory instance from a valid region should succeed.
  std::unique_ptr<SharedMemory> shm1 =
      display->CreateSharedMemory(std::move(shared_memory));
  EXPECT_TRUE(shm1);

  // Creating a shared memory instance from a invalid region should fail.
  std::unique_ptr<SharedMemory> shm2 =
      display->CreateSharedMemory(base::UnsafeSharedMemoryRegion());
  EXPECT_FALSE(shm2);
}

#if defined(USE_OZONE)
// The test crashes: crbug.com/622724
TEST_F(DisplayTest, DISABLED_CreateLinuxDMABufBuffer) {
  const gfx::Size buffer_size(256, 256);

  std::unique_ptr<Display> display(new Display);
  // Creating a prime buffer from a native pixmap handle should succeed.
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullAcceleratedWidget, VK_NULL_HANDLE,
                               buffer_size, gfx::BufferFormat::RGBA_8888,
                               gfx::BufferUsage::GPU_READ);
  gfx::NativePixmapHandle native_pixmap_handle = pixmap->ExportHandle();
  std::unique_ptr<Buffer> buffer1 = display->CreateLinuxDMABufBuffer(
      buffer_size, gfx::BufferFormat::RGBA_8888,
      std::move(native_pixmap_handle), false);
  EXPECT_TRUE(buffer1);

  // Create a handle without a file descriptor.
  native_pixmap_handle = pixmap->ExportHandle();
  native_pixmap_handle.planes[0].fd.reset();

  // Creating a prime buffer using an invalid fd should fail.
  std::unique_ptr<Buffer> buffer2 = display->CreateLinuxDMABufBuffer(
      buffer_size, gfx::BufferFormat::RGBA_8888,
      std::move(native_pixmap_handle), false);
  EXPECT_FALSE(buffer2);
}

// TODO(dcastagna): Add YV12 unittest once we can allocate the buffer
// via Ozone. crbug.com/618516

#endif

TEST_F(DisplayTest, CreateShellSurface) {
  std::unique_ptr<Display> display(new Display);

  // Create two surfaces.
  std::unique_ptr<Surface> surface1 = display->CreateSurface();
  ASSERT_TRUE(surface1);
  std::unique_ptr<Surface> surface2 = display->CreateSurface();
  ASSERT_TRUE(surface2);

  // Create a shell surface for surface1.
  std::unique_ptr<ShellSurface> shell_surface1 =
      display->CreateShellSurface(surface1.get());
  EXPECT_TRUE(shell_surface1);

  // Create a shell surface for surface2.
  std::unique_ptr<ShellSurface> shell_surface2 =
      display->CreateShellSurface(surface2.get());
  EXPECT_TRUE(shell_surface2);
}

TEST_F(DisplayTest, CreateClientControlledShellSurface) {
  std::unique_ptr<Display> display(new Display);

  // Create two surfaces.
  std::unique_ptr<Surface> surface1 = display->CreateSurface();
  ASSERT_TRUE(surface1);
  std::unique_ptr<Surface> surface2 = display->CreateSurface();
  ASSERT_TRUE(surface2);

  // Create a remote shell surface for surface1.
  std::unique_ptr<ClientControlledShellSurface> shell_surface1 =
      display->CreateClientControlledShellSurface(
          surface1.get(), ash::kShellWindowId_SystemModalContainer,
          2.0 /* default_scale_factor */);
  ASSERT_TRUE(shell_surface1);
  EXPECT_EQ(shell_surface1->scale(), 2.0);

  // Create a remote shell surface for surface2.
  std::unique_ptr<ShellSurfaceBase> shell_surface2 =
      display->CreateClientControlledShellSurface(
          surface2.get(), ash::desks_util::GetActiveDeskContainerId(),
          1.0 /* default_scale_factor */);
  EXPECT_TRUE(shell_surface2);
}

TEST_F(DisplayTest, CreateSubSurface) {
  std::unique_ptr<Display> display(new Display);

  // Create child, parent and toplevel surfaces.
  std::unique_ptr<Surface> child = display->CreateSurface();
  ASSERT_TRUE(child);
  std::unique_ptr<Surface> parent = display->CreateSurface();
  ASSERT_TRUE(parent);
  std::unique_ptr<Surface> toplevel = display->CreateSurface();
  ASSERT_TRUE(toplevel);

  // Attempting to create a sub surface for child with child as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(child.get(), child.get()));

  // Create a sub surface for child.
  std::unique_ptr<SubSurface> child_sub_surface =
      display->CreateSubSurface(child.get(), toplevel.get());
  EXPECT_TRUE(child_sub_surface);

  // Attempting to create another sub surface when already assigned the role of
  // sub surface should fail.
  EXPECT_FALSE(display->CreateSubSurface(child.get(), parent.get()));

  // Deleting the sub surface should allow a new sub surface to be created.
  child_sub_surface.reset();
  child_sub_surface = display->CreateSubSurface(child.get(), parent.get());
  EXPECT_TRUE(child_sub_surface);

  std::unique_ptr<Surface> sibling = display->CreateSurface();
  ASSERT_TRUE(sibling);

  // Create a sub surface for sibiling.
  std::unique_ptr<SubSurface> sibling_sub_surface =
      display->CreateSubSurface(sibling.get(), parent.get());
  EXPECT_TRUE(sibling_sub_surface);

  // Create a shell surface for toplevel surface.
  std::unique_ptr<ShellSurface> shell_surface =
      display->CreateShellSurface(toplevel.get());
  EXPECT_TRUE(shell_surface);

  // Attempting to create a sub surface when already assigned the role of
  // shell surface should fail.
  EXPECT_FALSE(display->CreateSubSurface(toplevel.get(), parent.get()));

  std::unique_ptr<Surface> grandchild = display->CreateSurface();
  ASSERT_TRUE(grandchild);
  // Create a sub surface for grandchild.
  std::unique_ptr<SubSurface> grandchild_sub_surface =
      display->CreateSubSurface(grandchild.get(), child.get());
  EXPECT_TRUE(grandchild_sub_surface);

  // Attempting to create a sub surface for parent with child as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(parent.get(), child.get()));

  // Attempting to create a sub surface for parent with grandchild as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(parent.get(), grandchild.get()));

  // Create a sub surface for parent.
  EXPECT_TRUE(display->CreateSubSurface(parent.get(), toplevel.get()));
}

class TestDataDeviceDelegate : public DataDeviceDelegate {
 public:
  // Overriden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* data_device) override {}
  DataOffer* OnDataOffer(DataOffer::Purpose purpose) override {
    return nullptr;
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& location,
               const DataOffer& data_offer) override {}
  void OnLeave() override {}
  void OnMotion(base::TimeTicks time_stamp,
                const gfx::PointF& location) override {}
  void OnDrop() override {}
  void OnSelection(const DataOffer& data_offer) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return false;
  }
};

class TestFileHelper : public FileHelper {
 public:
  // Overriden from TestFileHelper:
  TestFileHelper() {}
  std::string GetMimeTypeForUriList() const override { return ""; }
  bool GetUrlFromPath(const std::string& app_id,
                      const base::FilePath& path,
                      GURL* out) override {
    return true;
  }
  bool HasUrlsInPickle(const base::Pickle& pickle) override { return false; }
  void GetUrlsFromPickle(const std::string& app_id,
                         const base::Pickle& pickle,
                         UrlsFromPickleCallback callback) override {}
};

TEST_F(DisplayTest, CreateDataDevice) {
  TestDataDeviceDelegate device_delegate;
  Display display(nullptr, nullptr, std::make_unique<TestFileHelper>());

  std::unique_ptr<DataDevice> device =
      display.CreateDataDevice(&device_delegate);
  EXPECT_TRUE(device.get());
}

TEST_F(DisplayTest, PinnedAlwaysOnTopWindow) {
  Display display;

  std::unique_ptr<Surface> surface = display.CreateSurface();
  ASSERT_TRUE(surface);

  std::unique_ptr<ClientControlledShellSurface> shell_surface =
      display.CreateClientControlledShellSurface(
          surface.get(), ash::desks_util::GetActiveDeskContainerId(),
          2.0 /* default_scale_factor */);
  ASSERT_TRUE(shell_surface);
  EXPECT_EQ(shell_surface->scale(), 2.0);

  // This should not crash
  shell_surface->SetAlwaysOnTop(true);
  shell_surface->SetPinned(ash::WindowPinType::kPinned);
}

}  // namespace
}  // namespace exo
