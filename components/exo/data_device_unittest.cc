// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/extended_drag_source.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/test_data_device_delegate.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"

namespace exo {
namespace {

using ::ui::mojom::DragOperation;

class TestSeat : public Seat {
 public:
  TestSeat() : Seat(std::make_unique<TestDataExchangeDelegate>()) {}

  TestSeat(const TestSeat&) = delete;
  TestSeat& operator=(const TestSeat&) = delete;

  void set_focused_surface(Surface* surface) { surface_ = surface; }

  // Overriden from Seat:
  Surface* GetFocusedSurface() override { return surface_; }

 private:
  raw_ptr<Surface, DanglingUntriaged> surface_ = nullptr;
};

class DataDeviceTest : public test::ExoTestBase {
 public:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<TestSeat>();
    device_ = std::make_unique<DataDevice>(&delegate_, seat_.get());
    data_.SetString(std::u16string(u"Test data"));
    surface_ = std::make_unique<Surface>();
  }

  void TearDown() override {
    surface_.reset();
    device_.reset();
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

 protected:
  test::TestDataDeviceDelegate delegate_;
  std::unique_ptr<TestSeat> seat_;
  std::unique_ptr<DataDevice> device_;
  ui::OSExchangeData data_;
  std::unique_ptr<Surface> surface_;

  const ui::OSExchangeData unused_data_;
  const ui::DropTargetEvent unused_drop_target_event_{
      unused_data_, gfx::PointF(), gfx::PointF(), 0};
};

TEST_F(DataDeviceTest, Destroy) {
  std::vector<test::DataEvent> events;
  device_.reset();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kDestroy, events[0]);
}

TEST_F(DataDeviceTest, DataEventsDrop) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&test::TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), true));
  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  DragOperation output_drag_op;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(DragOperation::kLink, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, DataEventsExit) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  device_->OnDragExited();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kLeave, events[0]);
}

TEST_F(DataDeviceTest, DeleteDataDeviceDuringDrop) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());
  device_->OnDragEntered(event);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { device_.reset(); }));
  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  DragOperation output_drag_op;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(DragOperation::kNone, output_drag_op);
}

TEST_F(DataDeviceTest, DeleteDataOfferDuringDrag) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  delegate_.DeleteDataOffer(false);

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            device_->OnDragUpdated(event).drag_operation);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  DragOperation output_drag_op;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DataOfferNotFinished) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&test::TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), false));

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  DragOperation output_drag_op;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(DragOperation::kNone, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, NotAcceptDataEventsForSurface) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  delegate_.set_can_accept_data_events_for_surface(false);

  device_->OnDragEntered(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            device_->OnDragUpdated(event).drag_operation);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  DragOperation output_drag_op;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DropCallback_Run) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&test::TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), true));

  DragOperation output_drag_op = DragOperation::kNone;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(DragOperation::kLink, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, DropCallback_Invalidated) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);

  delegate_.DeleteDataOffer(false);

  DragOperation output_drag_op = DragOperation::kNone;
  std::move(drop_cb).Run(/*data=*/nullptr, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(DragOperation::kNone, output_drag_op);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DropCallback_Reset) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<test::DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback(unused_drop_target_event_);
  drop_cb.Reset();

  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kLeave, events[0]);
}

TEST_F(DataDeviceTest, ClipboardCopy) {
  // Selection event sent when getting a focus.
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  std::vector<test::DataEvent> events;
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kSelection, events[1]);

  // Next focus does not send selection.
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  // Clipboard change
  device_->OnClipboardDataChanged();
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kSelection, events[1]);

  // Losing focus does not create events.
  device_->OnSurfaceFocused(nullptr, nullptr, true);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, ClipboardCopyWithoutFocus) {
  device_->OnClipboardDataChanged();
  std::vector<test::DataEvent> events;
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, ClipboardDeviceCreatedAfterFocus) {
  seat_->set_focused_surface(surface_.get());
  device_.reset();
  std::vector<test::DataEvent> events;
  delegate_.PopEvents(&events);

  device_ = std::make_unique<DataDevice>(&delegate_, seat_.get());

  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(test::DataEvent::kOffer, events[0]);
  EXPECT_EQ(test::DataEvent::kSelection, events[1]);
}

TEST_F(DataDeviceTest, ClipboardFocusedSurfaceDestroyed) {
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  surface_.reset();
  std::vector<test::DataEvent> events;
  delegate_.PopEvents(&events);

  device_->OnClipboardDataChanged();
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

}  // namespace
}  // namespace exo
