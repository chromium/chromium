// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include <fcntl.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/data_device.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/file_helper.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

using DataOfferTest = test::ExoTestBase;

class TestDataOfferDelegate : public DataOfferDelegate {
 public:
  TestDataOfferDelegate() {}

  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  void OnDataOfferDestroying(DataOffer* offer) override {}

  // Called when |mime_type| is offered by the client.
  void OnOffer(const std::string& mime_type) override {
    mime_types_.insert(mime_type);
  }

  // Called when possible |source_actions| is offered by the client.
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {
    source_actions_ = source_actions;
  }

  // Called when current |action| is offered by the client.
  void OnAction(DndAction dnd_action) override { dnd_action_ = dnd_action; }

  const base::flat_set<std::string>& mime_types() const { return mime_types_; }
  const base::flat_set<DndAction>& source_actions() const {
    return source_actions_;
  }
  DndAction dnd_action() const { return dnd_action_; }

 private:
  base::flat_set<std::string> mime_types_;
  base::flat_set<DndAction> source_actions_;
  DndAction dnd_action_ = DndAction::kNone;

  DISALLOW_COPY_AND_ASSIGN(TestDataOfferDelegate);
};

class TestFileHelper : public FileHelper {
 public:
  TestFileHelper() = default;

  // Overridden from FileHelper:
  std::string GetMimeTypeForUriList() const override { return "text/uri-list"; }
  bool GetUrlFromPath(const std::string& app_id,
                      const base::FilePath& path,
                      GURL* out) override {
    *out = GURL("file://" + path.AsUTF8Unsafe());
    return true;
  }
  bool HasUrlsInPickle(const base::Pickle& pickle) override { return true; }
  void GetUrlsFromPickle(const std::string& app_id,
                         const base::Pickle& pickle,
                         UrlsFromPickleCallback callback) override {
    callback_ = std::move(callback);
  }

  void RunUrlsCallback(std::vector<GURL> urls) {
    std::move(callback_).Run(urls);
  }

 private:
  UrlsFromPickleCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestFileHelper);
};

bool ReadString(base::ScopedFD fd, std::string* out) {
  std::array<char, 128> buffer;
  char* it = buffer.begin();
  while (it != buffer.end()) {
    int result = read(fd.get(), it, buffer.end() - it);
    PCHECK(-1 != result);
    if (result == 0)
      break;
    it += result;
  }
  *out = std::string(reinterpret_cast<char*>(buffer.data()),
                     (it - buffer.begin()) / sizeof(char));
  return true;
}

bool ReadString16(base::ScopedFD fd, base::string16* out) {
  std::array<char, 128> buffer;
  char* it = buffer.begin();
  while (it != buffer.end()) {
    int result = read(fd.get(), it, buffer.end() - it);
    PCHECK(-1 != result);
    if (result == 0)
      break;
    it += result;
  }
  *out = base::string16(reinterpret_cast<base::char16*>(buffer.data()),
                        (it - buffer.begin()) / sizeof(base::char16));
  return true;
}

TEST_F(DataOfferTest, SetTextDropData) {
  base::flat_set<DndAction> source_actions;
  source_actions.insert(DndAction::kCopy);
  source_actions.insert(DndAction::kMove);

  ui::OSExchangeData data;
  data.SetString(base::string16(base::ASCIIToUTF16("Test data")));

  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestFileHelper file_helper;
  data_offer.SetDropData(&file_helper, data);
  data_offer.SetSourceActions(source_actions);
  data_offer.SetActions(base::flat_set<DndAction>(), DndAction::kMove);

  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(2u, delegate.source_actions().size());
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kCopy));
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kMove));
  EXPECT_EQ(DndAction::kMove, delegate.dnd_action());
}

TEST_F(DataOfferTest, SetHTMLDropData) {
  const std::string html_data = "Test HTML data 🔥 ❄";

  base::flat_set<DndAction> source_actions;
  source_actions.insert(DndAction::kCopy);
  source_actions.insert(DndAction::kMove);

  ui::OSExchangeData data;
  data.SetHtml(base::UTF8ToUTF16(html_data), GURL());

  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestFileHelper file_helper;
  data_offer.SetDropData(&file_helper, data);
  data_offer.SetSourceActions(source_actions);
  data_offer.SetActions(base::flat_set<DndAction>(), DndAction::kMove);

  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-16"));
  EXPECT_EQ(2u, delegate.source_actions().size());
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kCopy));
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kMove));
  EXPECT_EQ(DndAction::kMove, delegate.dnd_action());

  base::ScopedFD read, write;
  std::string result;
  EXPECT_TRUE(base::CreatePipe(&read, &write));
  data_offer.Receive("text/html;charset=utf-8", std::move(write));
  ReadString(std::move(read), &result);
  EXPECT_EQ(result, html_data);

  base::string16 result16;
  EXPECT_TRUE(base::CreatePipe(&read, &write));
  data_offer.Receive("text/html;charset=utf-16", std::move(write));
  ReadString16(std::move(read), &result16);
  EXPECT_EQ(result16, base::UTF8ToUTF16(html_data));
}

TEST_F(DataOfferTest, SetFileDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;
  data.SetFilename(base::FilePath("/test/downloads/file"));
  data_offer.SetDropData(&file_helper, data);

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/uri-list"));
}

TEST_F(DataOfferTest, SetPickleDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&file_helper, data);

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/uri-list"));
}

TEST_F(DataOfferTest, ReceiveString) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;
  data.SetString(base::ASCIIToUTF16("Test data"));
  data_offer.SetDropData(&file_helper, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/plain", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  base::ScopedFD read_pipe_16;
  base::ScopedFD write_pipe_16;
  ASSERT_TRUE(base::CreatePipe(&read_pipe_16, &write_pipe_16));
  data_offer.Receive("text/plain;charset=utf-16", std::move(write_pipe_16));
  base::string16 result_16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe_16), &result_16));
  EXPECT_EQ(base::ASCIIToUTF16("Test data"), result_16);

  base::ScopedFD read_pipe_8;
  base::ScopedFD write_pipe_8;
  ASSERT_TRUE(base::CreatePipe(&read_pipe_8, &write_pipe_8));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe_8));
  std::string result_8;
  ASSERT_TRUE(ReadString(std::move(read_pipe_8), &result_8));
  EXPECT_EQ("Test data", result_8);
}

TEST_F(DataOfferTest, ReceiveHTML) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;
  data.SetHtml(base::ASCIIToUTF16("Test HTML data"), GURL());
  data_offer.SetDropData(&file_helper, data);

  base::ScopedFD read_pipe_16;
  base::ScopedFD write_pipe_16;
  ASSERT_TRUE(base::CreatePipe(&read_pipe_16, &write_pipe_16));
  data_offer.Receive("text/html;charset=utf-16", std::move(write_pipe_16));
  base::string16 result_16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe_16), &result_16));
  EXPECT_EQ(base::ASCIIToUTF16("Test HTML data"), result_16);

  base::ScopedFD read_pipe_8;
  base::ScopedFD write_pipe_8;
  ASSERT_TRUE(base::CreatePipe(&read_pipe_8, &write_pipe_8));
  data_offer.Receive("text/html;charset=utf-8", std::move(write_pipe_8));
  std::string result_8;
  ASSERT_TRUE(ReadString(std::move(read_pipe_8), &result_8));
  EXPECT_EQ("Test HTML data", result_8);
}

TEST_F(DataOfferTest, ReceiveUriList) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;
  data.SetFilename(base::FilePath("/test/downloads/file"));
  data_offer.SetDropData(&file_helper, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/uri-list", std::move(write_pipe));
  base::string16 result;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result));
  EXPECT_EQ(base::ASCIIToUTF16("file:///test/downloads/file"), result);
}

TEST_F(DataOfferTest, ReceiveUriListFromPickle_ReceiveAfterUrlIsResolved) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&file_helper, data);

  // Run callback with a resolved URL.
  std::vector<GURL> urls;
  urls.push_back(
      GURL("content://org.chromium.arc.chromecontentprovider/path/to/file1"));
  file_helper.RunUrlsCallback(urls);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  // Receive is called after UrlsCallback runs.
  data_offer.Receive("text/uri-list", std::move(write_pipe));
  base::string16 result;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result));
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "content://org.chromium.arc.chromecontentprovider/path/to/file1"),
      result);
}

TEST_F(DataOfferTest, ReceiveUriListFromPickle_ReceiveBeforeUrlIsResolved) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&file_helper, data);

  base::ScopedFD read_pipe1;
  base::ScopedFD write_pipe1;
  ASSERT_TRUE(base::CreatePipe(&read_pipe1, &write_pipe1));
  base::ScopedFD read_pipe2;
  base::ScopedFD write_pipe2;
  ASSERT_TRUE(base::CreatePipe(&read_pipe2, &write_pipe2));

  // Receive is called (twice) before UrlsFromPickleCallback runs.
  data_offer.Receive("text/uri-list", std::move(write_pipe1));
  data_offer.Receive("text/uri-list", std::move(write_pipe2));

  // Run callback with a resolved URL.
  std::vector<GURL> urls;
  urls.push_back(
      GURL("content://org.chromium.arc.chromecontentprovider/path/to/file1"));
  file_helper.RunUrlsCallback(urls);

  base::string16 result1;
  ASSERT_TRUE(ReadString16(std::move(read_pipe1), &result1));
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "content://org.chromium.arc.chromecontentprovider/path/to/file1"),
      result1);
  base::string16 result2;
  ASSERT_TRUE(ReadString16(std::move(read_pipe2), &result2));
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "content://org.chromium.arc.chromecontentprovider/path/to/file1"),
      result2);
}

TEST_F(DataOfferTest,
       ReceiveUriListFromPickle_ReceiveBeforeEmptyUrlIsReturned) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::DRAG_DROP);

  TestFileHelper file_helper;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&file_helper, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  // Receive is called before UrlsCallback runs.
  data_offer.Receive("text/uri-list", std::move(write_pipe));

  // Run callback with an empty URL.
  std::vector<GURL> urls;
  urls.push_back(GURL(""));
  file_helper.RunUrlsCallback(urls);

  base::string16 result;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result));
  EXPECT_EQ(base::ASCIIToUTF16(""), result);
}

TEST_F(DataOfferTest, SetClipboardDataPlainText) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::COPY_PASTE);

  TestFileHelper file_helper;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(base::UTF8ToUTF16("Test data"));
  }
  data_offer.SetClipboardData(&file_helper,
                              *ui::Clipboard::GetForCurrentThread());

  EXPECT_EQ(3u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-16", std::move(write_pipe));
  base::string16 result16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result16));
  EXPECT_EQ("Test data", base::UTF16ToUTF8(result16));
}

TEST_F(DataOfferTest, SetClipboardDataHTML) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::COPY_PASTE);

  TestFileHelper file_helper;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(base::UTF8ToUTF16("Test data"), "");
  }
  data_offer.SetClipboardData(&file_helper,
                              *ui::Clipboard::GetForCurrentThread());

  EXPECT_EQ(2u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-16"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/html;charset=utf-8", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/html;charset=utf-16", std::move(write_pipe));
  base::string16 result16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result16));
  EXPECT_EQ("Test data", base::UTF16ToUTF8(result16));
}

TEST_F(DataOfferTest, SetClipboardDataRTF) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::COPY_PASTE);

  TestFileHelper file_helper;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteRTF("Test data");
  }
  data_offer.SetClipboardData(&file_helper,
                              *ui::Clipboard::GetForCurrentThread());

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/rtf"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/rtf", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);
}

TEST_F(DataOfferTest, AcceptWithNull) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate, DataOffer::Purpose::COPY_PASTE);
  data_offer.Accept(nullptr);
}

}  // namespace
}  // namespace exo
