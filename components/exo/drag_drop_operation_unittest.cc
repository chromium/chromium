// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/drag_drop_operation.h"

#include <memory>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "components/exo/buffer.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/gfx/geometry/point_f.h"

namespace exo {
namespace {

constexpr char kText[] = "test";
constexpr char kTextMimeType[] = "text/plain";

}  // namespace

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  // DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* source) override {}

  void OnTarget(const base::Optional<std::string>& mime_type) override {}

  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    base::WriteFileDescriptor(fd.get(), kText, sizeof(kText));
  }

  void OnCancelled() override {}

  void OnDndDropPerformed() override {}

  void OnDndFinished() override {}

  void OnAction(DndAction dnd_action) override {}

  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return true;
  }
};

class DragDropOperationTest : public test::ExoTestBase,
                              public aura::client::DragDropClientObserver {
 public:
  DragDropOperationTest() = default;
  ~DragDropOperationTest() override = default;
  DragDropOperationTest(const DragDropOperationTest&) = delete;
  DragDropOperationTest& operator=(const DragDropOperationTest&) = delete;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }

  void TearDown() override {
    aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
    test::ExoTestBase::TearDown();
  }

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(drag_blocked_callback_));
  }

  void OnDragEnded() override {}

 protected:
  void set_drag_blocked_callback(base::OnceClosure callback) {
    drag_blocked_callback_ = std::move(callback);
  }

 private:
  // Callback running inside the nested RunLoop in StartDragAndDrop().
  base::OnceClosure drag_blocked_callback_;
};

TEST_F(DragDropOperationTest, DeleteDuringDragging) {
  auto delegate = std::make_unique<TestDataSourceDelegate>();
  auto data_source = std::make_unique<DataSource>(delegate.get());
  data_source->Offer(kTextMimeType);

  auto origin_surface = std::make_unique<Surface>();
  ash::Shell::GetPrimaryRootWindow()->AddChild(origin_surface->window());

  gfx::Size buffer_size(100, 100);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  auto icon_surface = std::make_unique<Surface>();
  icon_surface->Attach(buffer.get());

  auto operation = DragDropOperation::Create(
      data_source.get(), origin_surface.get(), icon_surface.get(),
      gfx::PointF(), ui::mojom::DragEventSource::kMouse);
  icon_surface->Commit();

  base::RunLoop run_loop;
  set_drag_blocked_callback(base::BindOnce(
      [](std::unique_ptr<DataSource> data_source,
         base::WeakPtr<DragDropOperation> operation,
         base::OnceClosure quit_closure) {
        // This function runs inside the nested RunLoop in
        // ash::DragDropController::StartDragAndDrop().
        EXPECT_TRUE(operation);
        // Deleting DataSource causes DragDropOperation to be deleted as well.
        data_source.reset();
        EXPECT_FALSE(operation);
        std::move(quit_closure).Run();
      },
      std::move(data_source), operation, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(operation);
}

}  // namespace exo
