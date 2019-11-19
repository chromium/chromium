// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/file_helper.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"

namespace exo {
namespace {

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

  size_t PopEvents(std::vector<DataEvent>* out) {
    out->swap(events_);
    events_.clear();
    return out->size();
  }
  Surface* entered_surface() const { return entered_surface_; }
  void DeleteDataOffer() { data_offer_.reset(); }
  void set_can_accept_data_events_for_surface(bool value) {
    can_accept_data_events_for_surface_ = value;
  }

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* data_device) override {
    events_.push_back(DataEvent::kDestroy);
  }
  DataOffer* OnDataOffer(DataOffer::Purpose purpose) override {
    events_.push_back(DataEvent::kOffer);
    data_offer_.reset(new DataOffer(new TestDataOfferDelegate, purpose));
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
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return can_accept_data_events_for_surface_;
  }

 private:
  std::vector<DataEvent> events_;
  std::unique_ptr<DataOffer> data_offer_;
  Surface* entered_surface_ = nullptr;
  bool can_accept_data_events_for_surface_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestDataDeviceDelegate);
};

class TestFileHelper : public FileHelper {
 public:
  TestFileHelper() = default;

  // Overridden from FileHelper:
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFileHelper);
};

class TestSeat : public Seat {
 public:
  TestSeat() {}
  void set_focused_surface(Surface* surface) { surface_ = surface; }

  // Overriden from Seat:
  Surface* GetFocusedSurface() override { return surface_; }

 private:
  Surface* surface_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestSeat);
};

class DataDeviceTest : public test::ExoTestBase {
 public:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<TestSeat>();
    device_ =
        std::make_unique<DataDevice>(&delegate_, seat_.get(), &file_helper_);
    data_.SetString(base::string16(base::ASCIIToUTF16("Test data")));
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
  TestFileHelper file_helper_;
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

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK, device_->OnDragUpdated(event));
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  device_->OnPerformDrop(event);
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

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK, device_->OnDragUpdated(event));
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kMotion, events[0]);

  device_->OnDragExited();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kLeave, events[0]);
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

  delegate_.DeleteDataOffer();

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, device_->OnDragUpdated(event));
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  device_->OnPerformDrop(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, NotAcceptDataEventsForSurface) {
  ui::DropTargetEvent event(data_, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DataEvent> events;
  delegate_.set_can_accept_data_events_for_surface(false);

  device_->OnDragEntered(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, device_->OnDragUpdated(event));
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  device_->OnPerformDrop(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, ClipboardCopy) {
  // Selection event sent when getting a focus.
  device_->OnSurfaceFocusing(surface_.get());
  std::vector<DataEvent> events;
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);

  // Next focus does not send selection.
  device_->OnSurfaceFocusing(surface_.get());
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  // Clipboard change
  device_->OnClipboardDataChanged();
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);

  // Losing focuse does not create events.
  device_->OnSurfaceFocusing(nullptr);
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

  device_ =
      std::make_unique<DataDevice>(&delegate_, seat_.get(), &file_helper_);

  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DataEvent::kOffer, events[0]);
  EXPECT_EQ(DataEvent::kSelection, events[1]);
}

TEST_F(DataDeviceTest, ClipboardFocusedSurfaceDestroyed) {
  device_->OnSurfaceFocusing(surface_.get());
  surface_.reset();
  std::vector<DataEvent> events;
  delegate_.PopEvents(&events);

  device_->OnClipboardDataChanged();
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

}  // namespace
}  // namespace exo
