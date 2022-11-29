// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/extended_drag_source.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"

namespace exo {
namespace {

using ::ui::mojom::DragOperation;

enum class DataEvent {
  kOffer,
  kEnter,
  kLeave,
  kMotion,
  kDrop,
  kDestroy,
  kSelection
};

class TestDataOfferDelegate : public DataOfferDelegate {
 public:
  ~TestDataOfferDelegate() override {}

  // Overridden from DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* offer) override { delete this; }
  void OnOffer(const std::string& mime_type) override {}
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {}
  void OnAction(DndAction action) override {}
};

class TestDataDeviceDelegate : public DataDeviceDelegate {
 public:
  TestDataDeviceDelegate() {}

  TestDataDeviceDelegate(const TestDataDeviceDelegate&) = delete;
  TestDataDeviceDelegate& operator=(const TestDataDeviceDelegate&) = delete;

  size_t PopEvents(std::vector<DataEvent>* out) {
    out->swap(events_);
    events_.clear();
    return out->size();
  }
  Surface* entered_surface() const { return entered_surface_; }
  void DeleteDataOffer(bool finished) {
    if (finished)
      data_offer_->Finish();
    data_offer_.reset();
  }
  void set_can_accept_data_events_for_surface(bool value) {
    can_accept_data_events_for_surface_ = value;
  }

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* data_device) override {
    events_.push_back(DataEvent::kDestroy);
  }
  DataOffer* OnDataOffer() override {
    events_.push_back(DataEvent::kOffer);
    data_offer_ = std::make_unique<DataOffer>(new TestDataOfferDelegate);
    return data_offer_.get();
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& location,
               const DataOffer& data_offer) override {
    events_.push_back(DataEvent::kEnter);
    entered_surface_ = surface;
  }
  void OnLeave() override { events_.push_back(DataEvent::kLeave); }
  void OnMotion(base::TimeTicks time_stamp,
                const gfx::PointF& location) override {
    events_.push_back(DataEvent::kMotion);
  }
  void OnDrop() override { events_.push_back(DataEvent::kDrop); }
  void OnSelection(const DataOffer& data_offer) override {
    events_.push_back(DataEvent::kSelection);
  }
  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return can_accept_data_events_for_surface_;
  }

 private:
  std::vector<DataEvent> events_;
  std::unique_ptr<DataOffer> data_offer_;
  Surface* entered_surface_ = nullptr;
  bool can_accept_data_events_for_surface_ = true;
};

class TestSeat : public Seat {
 public:
  TestSeat() : Seat(std::make_unique<TestDataExchangeDelegate>()) {}

  TestSeat(const TestSeat&) = delete;
  TestSeat& operator=(const TestSeat&) = delete;

  void set_focused_surface(Surface* surface) { surface_ = surface; }

  // Overriden from Seat:
  Surface* GetFocusedSurface() override { return surface_; }

 private:
  Surface* surface_ = nullptr;
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
  TestDataDeviceDelegate delegate_;
  std::unique_ptr<TestSeat> seat_;
  std::unique_ptr<DataDevice> device_;
  ui::OSExchangeData data_;
  std::unique_ptr<Surface> surface_;
};

TEST_F(DataDeviceTest, Destroy) {
  std::vector<DataEvent> events;
  device_.reset();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kDestroy, events[0]);
}

TEST_F(DataDeviceTest, DataEventsDrop) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), true));

  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(DragOperation::kLink, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kDrop, events[0]);
}

// Helper class to plumb the ExtendedDragSource instance.
class TestExtendedDragSourceDelegate : public ExtendedDragSource::Delegate {
 public:
  TestExtendedDragSourceDelegate() = default;
  TestExtendedDragSourceDelegate(const TestExtendedDragSourceDelegate&) =
      delete;
  TestExtendedDragSourceDelegate& operator=(
      const TestExtendedDragSourceDelegate&) = delete;
  ~TestExtendedDragSourceDelegate() override = default;

  // ExtendedDragSource::Delegate:
  bool ShouldAllowDropAnywhere() const override { return false; }
  bool ShouldLockCursor() const override { return false; }

  void OnSwallowed(const std::string& mime_type) override {}

  void OnUnswallowed(const std::string& mime_type,
                     const gfx::Vector2d& offset) override {}
  void OnDataSourceDestroying() override {}
};

// Helper class to plumb the DataSource instance.
class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() = default;
  ~TestDataSourceDelegate() override = default;

  void OnDataSourceDestroying(DataSource* source) override {}
  void OnTarget(const absl::optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {}
  void OnCancelled() override {}
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return true;
  }
};

TEST_F(DataDeviceTest, DataEventsPreventMotion) {
  // Create a DataDevice with a focused Surface.
  seat_->set_focused_surface(surface_.get());
  device_.reset();
  std::vector<DataEvent> events;
  delegate_.PopEvents(&events);
  device_ = std::make_unique<DataDevice>(&delegate_, seat_.get());

  // Start a drag operation.
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  device_->OnDragEntered(event);
  delegate_.PopEvents(&events);

  // Minic a window detach (new Surface creation).
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* other_surface = shell_surface->root_surface();

  device_->OnSurfaceFocused(other_surface, nullptr, true);
  delegate_.PopEvents(&events);

  // Mimic an extended_drag_source drag operation.
  TestDataSourceDelegate data_source_delegate;
  auto data_source = std::make_unique<DataSource>(&data_source_delegate);
  TestExtendedDragSourceDelegate extended_drag_source_delegate;
  auto extended_drag_source = std::make_unique<ExtendedDragSource>(
      data_source.get(), &extended_drag_source_delegate);
  extended_drag_source->Drag(other_surface, gfx::Vector2d());

  // Prevent drag.motion events to be sent.
  other_surface->window()->GetToplevelWindow()->SetProperty(
      chromeos::kCanAttachToAnotherWindowKey, false);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(0u, delegate_.PopEvents(&events));

  other_surface->window()->GetToplevelWindow()->ClearProperty(
      chromeos::kCanAttachToAnotherWindowKey);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), true));

  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(DragOperation::kLink, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, DataEventsExit) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  device_->OnDragExited();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kLeave, events[0]);
}

TEST_F(DataDeviceTest, DeleteDataDeviceDuringDrop) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());
  device_->OnDragEntered(event);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { device_.reset(); }));
  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(DragOperation::kNone, output_drag_op);
}

TEST_F(DataDeviceTest, DeleteDataOfferDuringDrag) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  delegate_.DeleteDataOffer(false);

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            device_->OnDragUpdated(event).drag_operation);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DataOfferNotFinished) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), false));

  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(DragOperation::kNone, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, NotAcceptDataEventsForSurface) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  delegate_.set_can_accept_data_events_for_surface(false);

  device_->OnDragEntered(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            device_->OnDragUpdated(event).drag_operation);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  auto drop_cb = device_->GetDropCallback();
  DragOperation output_drag_op;
  std::move(drop_cb).Run(output_drag_op);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DropCallback_Run) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestDataDeviceDelegate::DeleteDataOffer,
                                base::Unretained(&delegate_), true));

  DragOperation output_drag_op = DragOperation::kNone;
  std::move(drop_cb).Run(output_drag_op);

  EXPECT_EQ(DragOperation::kLink, output_drag_op);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, DropCallback_Invalidated) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback();

  delegate_.DeleteDataOffer(false);

  DragOperation output_drag_op = DragOperation::kNone;
  std::move(drop_cb).Run(output_drag_op);

  EXPECT_EQ(DragOperation::kNone, output_drag_op);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, DropCallback_Reset) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            device_->OnDragUpdated(event).drag_operation);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  auto drop_cb = device_->GetDropCallback();
  drop_cb.Reset();

  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kLeave, events[0]);
}

TEST_F(DataDeviceTest, ClipboardCopy) {
  // Selection event sent when getting a focus.
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  std::vector<DataEvent> events;
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);

  // Next focus does not send selection.
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  // Clipboard change
  device_->OnClipboardDataChanged();
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);

  // Losing focus does not create events.
  device_->OnSurfaceFocused(nullptr, nullptr, true);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, ClipboardCopyWithoutFocus) {
  device_->OnClipboardDataChanged();
  std::vector<DataEvent> events;
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, ClipboardDeviceCreatedAfterFocus) {
  seat_->set_focused_surface(surface_.get());
  device_.reset();
  std::vector<DataEvent> events;
  delegate_.PopEvents(&events);

  device_ = std::make_unique<DataDevice>(&delegate_, seat_.get());

  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);
}

TEST_F(DataDeviceTest, ClipboardFocusedSurfaceDestroyed) {
  device_->OnSurfaceFocused(surface_.get(), nullptr, true);
  surface_.reset();
  std::vector<DataEvent> events;
  delegate_.PopEvents(&events);

  device_->OnClipboardDataChanged();
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

}  // namespace
}  // namespace exo
