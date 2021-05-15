// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/seat_observer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace exo {
namespace {

using SeatTest = test::ExoTestBase;

class MockSeatObserver : public SeatObserver {
 public:
  int on_surface_focused_count() { return on_surface_focused_count_; }

  // Overridden from SeatObserver:
  void OnSurfaceFocusing(Surface* gaining_focus) override {
    ASSERT_EQ(on_surface_focused_count_, on_surface_pre_focused_count_);
    on_surface_pre_focused_count_++;
  }
  void OnSurfaceFocused(Surface* gained_focus) override {
    on_surface_focused_count_++;
    ASSERT_EQ(on_surface_focused_count_, on_surface_pre_focused_count_);
  }

 private:
  int on_surface_pre_focused_count_ = 0;
  int on_surface_focused_count_ = 0;
};

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() {}
  bool cancelled() const { return cancelled_; }

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override {}
  void OnTarget(const absl::optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    if (!data_.has_value()) {
      const char kTestData[] = "TestData";
      ASSERT_TRUE(base::WriteFileDescriptor(fd.get(), kTestData));
    } else {
      ASSERT_TRUE(base::WriteFileDescriptor(fd.get(), *data_));
    }
  }
  void OnCancelled() override { cancelled_ = true; }
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) const override {
    return can_accept_;
  }

  void SetData(std::vector<uint8_t> data) { data_ = std::move(data); }

  bool can_accept_ = true;

 private:
  bool cancelled_ = false;
  absl::optional<std::vector<uint8_t>> data_;

  DISALLOW_COPY_AND_ASSIGN(TestDataSourceDelegate);
};

void RunReadingTask() {
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

class TestSeat : public Seat {
 public:
  TestSeat() : Seat(std::make_unique<TestDataExchangeDelegate>()) {}

  TestSeat(const TestSeat&) = delete;
  void operator=(const TestSeat&) = delete;

  void set_focused_surface(Surface* surface) { surface_ = surface; }

  // Seat:
  Surface* GetFocusedSurface() override { return surface_; }

 private:
  Surface* surface_ = nullptr;
};

TEST_F(SeatTest, OnSurfaceFocused) {
  TestSeat seat;
  MockSeatObserver observer;

  seat.AddObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());

  seat.RemoveObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());
}

TEST_F(SeatTest, SetSelection) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  RunReadingTask();

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);

  EXPECT_EQ(clipboard, std::string("TestData"));
}

TEST_F(SeatTest, SetSelectionTextUTF8) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  // UTF8 encoded data
  const uint8_t data[] = {
      0xe2, 0x9d, 0x84,       // SNOWFLAKE
      0xf0, 0x9f, 0x94, 0xa5  // FIRE
  };
  std::u16string converted_data;
  EXPECT_TRUE(base::UTF8ToUTF16(reinterpret_cast<const char*>(data),
                                sizeof(data), &converted_data));

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  source.Offer("text/html;charset=utf-8");
  delegate.SetData(std::vector<uint8_t>(data, data + sizeof(data)));
  seat.SetSelection(&source);

  RunReadingTask();

  std::u16string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, converted_data);

  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard, &url,
      &start, &end);
  EXPECT_EQ(clipboard, converted_data);
}

TEST_F(SeatTest, SetSelectionTextUTF8Legacy) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  // UTF8 encoded data
  const uint8_t data[] = {
      0xe2, 0x9d, 0x84,       // SNOWFLAKE
      0xf0, 0x9f, 0x94, 0xa5  // FIRE
  };
  std::u16string converted_data;
  EXPECT_TRUE(base::UTF8ToUTF16(reinterpret_cast<const char*>(data),
                                sizeof(data), &converted_data));

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("UTF8_STRING");
  delegate.SetData(std::vector<uint8_t>(data, data + sizeof(data)));
  seat.SetSelection(&source);

  RunReadingTask();

  std::u16string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, converted_data);
}

TEST_F(SeatTest, SetSelectionTextUTF16LE) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  // UTF16 little endian encoded data
  const uint8_t data[] = {
      0xff, 0xfe,              // Byte order mark
      0x44, 0x27,              // SNOWFLAKE
      0x3d, 0xd8, 0x25, 0xdd,  // FIRE
  };
  std::u16string converted_data;
  converted_data.push_back(0x2744);
  converted_data.push_back(0xd83d);
  converted_data.push_back(0xdd25);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-16");
  source.Offer("text/html;charset=utf-16");
  delegate.SetData(std::vector<uint8_t>(data, data + sizeof(data)));
  seat.SetSelection(&source);

  RunReadingTask();

  std::u16string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, converted_data);

  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard, &url,
      &start, &end);
  EXPECT_EQ(clipboard, converted_data);
}

TEST_F(SeatTest, SetSelectionTextUTF16BE) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  // UTF16 big endian encoded data
  const uint8_t data[] = {
      0xfe, 0xff,              // Byte order mark
      0x27, 0x44,              // SNOWFLAKE
      0xd8, 0x3d, 0xdd, 0x25,  // FIRE
  };
  std::u16string converted_data;
  converted_data.push_back(0x2744);
  converted_data.push_back(0xd83d);
  converted_data.push_back(0xdd25);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-16");
  source.Offer("text/html;charset=utf-16");
  delegate.SetData(std::vector<uint8_t>(data, data + sizeof(data)));
  seat.SetSelection(&source);

  RunReadingTask();

  std::u16string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, converted_data);

  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard, &url,
      &start, &end);
  EXPECT_EQ(clipboard, converted_data);
}

TEST_F(SeatTest, SetSelectionTextEmptyString) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  const uint8_t data[] = {};

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  source.Offer("text/html;charset=utf-16");
  delegate.SetData(std::vector<uint8_t>(data, data + sizeof(data)));
  seat.SetSelection(&source);

  RunReadingTask();

  std::u16string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard.size(), 0u);

  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard, &url,
      &start, &end);
  EXPECT_EQ(clipboard.size(), 0u);
}

TEST_F(SeatTest, SetSelectionRTF) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/rtf");
  seat.SetSelection(&source);

  RunReadingTask();

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadRTF(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);

  EXPECT_EQ(clipboard, std::string("TestData"));
}

TEST_F(SeatTest, SetSelectionFilenames) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kClipboardFilenames}, {});

  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  std::string data("file:///path1\r\nfile:///path2");

  TestDataSourceDelegate delegate;
  delegate.SetData(std::vector<uint8_t>(data.begin(), data.end()));
  DataSource source(&delegate);
  source.Offer("text/uri-list");
  seat.SetSelection(&source);

  RunReadingTask();

  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr, &filenames);

  EXPECT_EQ(ui::FileInfosToURIList(filenames), data);
}

TEST_F(SeatTest, SetSelectionFilenamesClipboardFilesDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({}, {features::kClipboardFilenames});

  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/uri-list");
  seat.SetSelection(&source);

  RunReadingTask();

  std::string data;
  ui::Clipboard::GetForCurrentThread()->ReadData(
      ui::ClipboardFormatType::GetWebCustomDataType(),
      /*data_dst=*/nullptr, &data);
  base::Pickle pickle(data.c_str(), data.size());
  std::string clipboard;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(iter.ReadString(&clipboard));

  EXPECT_EQ(clipboard, std::string("TestData"));
}

TEST_F(SeatTest, SetSelection_TwiceSame) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  seat.SetSelection(&source);
  RunReadingTask();
  seat.SetSelection(&source);
  RunReadingTask();

  EXPECT_FALSE(delegate.cancelled());
}

TEST_F(SeatTest, SetSelection_TwiceDifferent) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate1;
  DataSource source1(&delegate1);
  seat.SetSelection(&source1);
  RunReadingTask();

  EXPECT_FALSE(delegate1.cancelled());

  TestDataSourceDelegate delegate2;
  DataSource source2(&delegate2);
  seat.SetSelection(&source2);
  RunReadingTask();

  EXPECT_TRUE(delegate1.cancelled());
}

TEST_F(SeatTest, SetSelection_ClipboardChangedDuringSetSelection) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  seat.SetSelection(&source);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"New data");
  }

  RunReadingTask();

  // The previous source should be cancelled.
  EXPECT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, "New data");
}

TEST_F(SeatTest, SetSelection_ClipboardChangedAfterSetSelection) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  seat.SetSelection(&source);
  RunReadingTask();

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"New data");
  }

  // The previous source should be cancelled.
  EXPECT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, "New data");
}

TEST_F(SeatTest, SetSelection_SourceDestroyedDuringSetSelection) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Original data");
  }

  {
    TestDataSourceDelegate delegate;
    DataSource source(&delegate);
    seat.SetSelection(&source);
    // source destroyed here.
  }

  RunReadingTask();

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, "Original data");
}

TEST_F(SeatTest, SetSelection_SourceDestroyedAfterSetSelection) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate1;
  {
    DataSource source(&delegate1);
    seat.SetSelection(&source);
    RunReadingTask();
    // source destroyed here.
  }

  RunReadingTask();

  {
    TestDataSourceDelegate delegate2;
    DataSource source(&delegate2);
    seat.SetSelection(&source);
    RunReadingTask();
    // source destroyed here.
  }

  RunReadingTask();

  // delegate1 should not receive cancel request because the first data source
  // has already been destroyed.
  EXPECT_FALSE(delegate1.cancelled());
}

TEST_F(SeatTest, SetSelection_NullSource) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  RunReadingTask();

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Golden data");
  }

  // Should not affect the current state of the clipboard.
  seat.SetSelection(nullptr);

  ASSERT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard);
  EXPECT_EQ(clipboard, "Golden data");
}

TEST_F(SeatTest, SetSelection_NoFocusedSurface) {
  TestSeat seat;
  seat.set_focused_surface(nullptr);

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  EXPECT_TRUE(delegate.cancelled());
}

TEST_F(SeatTest, SetSelection_ClientOutOfFocus) {
  TestSeat seat;
  Surface focused_surface;
  seat.set_focused_surface(&focused_surface);

  TestDataSourceDelegate delegate;
  delegate.can_accept_ = false;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  EXPECT_TRUE(delegate.cancelled());
}

TEST_F(SeatTest, PressedKeys) {
  TestSeat seat;
  ui::KeyEvent press_a(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0);
  ui::KeyEvent release_a(ui::ET_KEY_RELEASED, ui::VKEY_A, ui::DomCode::US_A, 0);
  ui::KeyEvent press_b(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0);
  ui::KeyEvent release_b(ui::ET_KEY_RELEASED, ui::VKEY_B, ui::DomCode::US_B, 0);

  // Press A, it should be in the map.
  seat.WillProcessEvent(&press_a);
  seat.OnKeyEvent(press_a.AsKeyEvent());
  seat.DidProcessEvent(&press_a);
  base::flat_map<ui::DomCode, KeyState> pressed_keys;
  pressed_keys[ui::CodeFromNative(&press_a)] = KeyState{press_a.code(), false};
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Press B, then A & B should be in the map.
  seat.WillProcessEvent(&press_b);
  seat.OnKeyEvent(press_b.AsKeyEvent());
  seat.DidProcessEvent(&press_b);
  pressed_keys[ui::CodeFromNative(&press_b)] = KeyState{press_b.code(), false};
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Release A, with the normal order where DidProcessEvent is after OnKeyEvent,
  // only B should be in the map.
  seat.WillProcessEvent(&release_a);
  seat.OnKeyEvent(release_a.AsKeyEvent());
  seat.DidProcessEvent(&release_a);
  pressed_keys.erase(ui::CodeFromNative(&press_a));
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Release B, do it out of order so DidProcessEvent is before OnKeyEvent, the
  // map should then be empty.
  seat.WillProcessEvent(&release_b);
  seat.DidProcessEvent(&release_b);
  seat.OnKeyEvent(release_b.AsKeyEvent());
  EXPECT_TRUE(seat.pressed_keys().empty());
}

TEST_F(SeatTest, DragDropAbort) {
  TestSeat seat;
  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  Surface origin, icon;

  // Give origin a root window for DragDropOperation.
  GetContext()->AddChild(origin.window());

  seat.StartDrag(&source, &origin, &icon, ui::mojom::DragEventSource::kMouse);
  EXPECT_TRUE(seat.get_drag_drop_operation_for_testing());
  seat.AbortPendingDragOperation();
  EXPECT_FALSE(seat.get_drag_drop_operation_for_testing());
}

}  // namespace
}  // namespace exo
