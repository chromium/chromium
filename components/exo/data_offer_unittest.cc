// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/data_offer.h"

#include <fcntl.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/exo/data_device.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/test_data_offer_delegate.h"
#include "components/exo/test/test_security_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace exo {
namespace {

using test::TestDataOfferDelegate;

class DataOfferTest : public test::ExoTestBase {
 public:
  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    test::ExoTestBase::TearDown();
  }
};

class TestDataTransferPolicyController : ui::DataTransferPolicyController {
 public:
  TestDataTransferPolicyController() = default;
  TestDataTransferPolicyController(TestDataTransferPolicyController&) = delete;
  TestDataTransferPolicyController& operator=(
      const TestDataTransferPolicyController&) = delete;

  ui::EndpointType last_src_type() const { return last_src_type_; }
  ui::EndpointType last_dst_type() const { return last_dst_type_; }

 private:
  // ui::DataTransferPolicyController:
  bool IsClipboardReadAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::optional<size_t> size) override {
    if (data_src.has_value()) {
      last_src_type_ = data_src->type();
    }
    last_dst_type_ = data_dst->type();
    return true;
  }

  void PasteIfAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
      content::RenderFrameHost* web_contents,
      base::OnceCallback<void(bool)> callback) override {}

  void DropIfAllowed(std::optional<ui::DataTransferEndpoint> data_src,
                     std::optional<ui::DataTransferEndpoint> data_dst,
                     std::optional<std::vector<ui::FileInfo>> filenames,
                     base::OnceClosure drop_cb) override {
    std::move(drop_cb).Run();
  }

  ui::EndpointType last_src_type_ = ui::EndpointType::kUnknownVm;
  ui::EndpointType last_dst_type_ = ui::EndpointType::kUnknownVm;
};

bool ReadString(base::ScopedFD fd, std::string* out) {
  std::array<char, 128> buffer;
  char* it = buffer.data();
  char* end = it + buffer.size();
  while (it != end) {
    int result = read(fd.get(), it, end - it);
    PCHECK(-1 != result);
    if (result == 0)
      break;
    it += result;
  }
  *out = std::string(reinterpret_cast<char*>(buffer.data()),
                     (it - buffer.data()) / sizeof(char));
  return true;
}

bool ReadString16(base::ScopedFD fd, std::u16string* out) {
  std::array<char, 128> buffer;
  char* it = buffer.data();
  char* end = it + buffer.size();
  while (it != it + buffer.size()) {
    int result = read(fd.get(), it, end - it);
    PCHECK(-1 != result);
    if (result == 0)
      break;
    it += result;
  }
  *out = std::u16string(reinterpret_cast<char16_t*>(buffer.data()),
                        (it - buffer.data()) / sizeof(char16_t));
  return true;
}

TEST_F(DataOfferTest, SetTextDropData) {
  base::flat_set<DndAction> source_actions;
  source_actions.insert(DndAction::kCopy);
  source_actions.insert(DndAction::kMove);

  ui::OSExchangeData data;
  data.SetString(std::u16string(u"Test data"));

  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestDataExchangeDelegate data_exchange_delegate;
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);
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
  DataOffer data_offer(&delegate);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestDataExchangeDelegate data_exchange_delegate;
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);
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

  std::u16string result16;
  EXPECT_TRUE(base::CreatePipe(&read, &write));
  data_offer.Receive("text/html;charset=utf-16", std::move(write));
  ReadString16(std::move(read), &result16);
  EXPECT_EQ(result16, base::UTF8ToUTF16(html_data));
}

TEST_F(DataOfferTest, SetFileDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  data.SetFilename(base::FilePath("/test/downloads/file"));
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/uri-list"));
}

TEST_F(DataOfferTest, SetPickleDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/uri-list"));
}

TEST_F(DataOfferTest, SetFileContentsDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  data.provider().SetFileContents(base::FilePath("\"test file\".jpg"),
                                  std::string("test data"));
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count(
                    "application/octet-stream;name=\"\\\"test file\\\".jpg\""));
}

TEST_F(DataOfferTest, ReceiveString) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  data.SetString(u"Test data");
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

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
  std::u16string result_16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe_16), &result_16));
  EXPECT_EQ(u"Test data", result_16);

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
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  data.SetHtml(u"Test HTML data", GURL());
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  base::ScopedFD read_pipe_16;
  base::ScopedFD write_pipe_16;
  ASSERT_TRUE(base::CreatePipe(&read_pipe_16, &write_pipe_16));
  data_offer.Receive("text/html;charset=utf-16", std::move(write_pipe_16));
  std::u16string result_16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe_16), &result_16));
  EXPECT_EQ(u"Test HTML data", result_16);

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
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  data.SetFilename(base::FilePath("/test/downloads/file"));
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/uri-list", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("file:///test/downloads/file", result);
}

TEST_F(DataOfferTest, ReceiveUriListFromPickle_ReceiveBeforeUrlIsResolved) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

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
  delegate.GetSecurityDelegate()->RunSendPickleCallback(urls);

  std::string result1;
  ASSERT_TRUE(ReadString(std::move(read_pipe1), &result1));
  EXPECT_EQ("content://org.chromium.arc.chromecontentprovider/path/to/file1",
            result1);
  std::string result2;
  ASSERT_TRUE(ReadString(std::move(read_pipe2), &result2));
  EXPECT_EQ("content://org.chromium.arc.chromecontentprovider/path/to/file1",
            result2);
}

TEST_F(DataOfferTest,
       ReceiveUriListFromPickle_ReceiveBeforeEmptyUrlIsReturned) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;

  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num files
  pickle.WriteString("filesystem:chrome-extension://path/to/file1");
  pickle.WriteInt64(1000);   // file size
  pickle.WriteString("id");  // filesystem id
  data.SetPickledData(
      ui::ClipboardFormatType::GetType("chromium/x-file-system-files"), pickle);
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  // Receive is called before UrlsCallback runs.
  data_offer.Receive("text/uri-list", std::move(write_pipe));

  // Run callback with an empty URL.
  std::vector<GURL> urls;
  urls.push_back(GURL(""));
  delegate.GetSecurityDelegate()->RunSendPickleCallback(urls);

  std::u16string result;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result));
  EXPECT_EQ(u"", result);
}

TEST_F(DataOfferTest, ReceiveFileContentsDropData) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  ui::OSExchangeData data;
  const std::string expected = "test data";
  data.provider().SetFileContents(base::FilePath("test.jpg"), expected);
  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("application/octet-stream;name=\"test.jpg\"",
                     std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ(expected, result);
}

TEST_F(DataOfferTest, SetClipboardDataPlainText) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(3u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  // Read a second time.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  // Read as utf-16.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-16", std::move(write_pipe));
  std::u16string result16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result16));
  EXPECT_EQ("Test data", base::UTF16ToUTF8(result16));
}

TEST_F(DataOfferTest, SetClipboardDataOfferDteToLacros) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kLacros);
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.SetDataSource(std::make_unique<ui::DataTransferEndpoint>(
        GURL("https://www.google.com")));
    writer.WriteText(u"Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(4u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));
  EXPECT_EQ(1u,
            delegate.mime_types().count("chromium/x-data-transfer-endpoint"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string text_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &text_result));
  EXPECT_EQ("Test data", text_result);

  // Retrieve encoded clipboard source data transfer endpoint.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("chromium/x-data-transfer-endpoint",
                     std::move(write_pipe));
  std::string dte_json_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &dte_json_result));
  EXPECT_EQ(
      "{\"endpoint_type\":\"url\","
      "\"off_the_record\":false,"
      "\"url\":\"https://www.google.com/\"}",
      dte_json_result);
}

TEST_F(DataOfferTest, SetClipboardDataDoNotOfferDteToNonLacros) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kArc);
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.SetDataSource(std::make_unique<ui::DataTransferEndpoint>(
        GURL("https://www.google.com")));
    writer.WriteText(u"Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(3u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));
  EXPECT_EQ(0u,
            delegate.mime_types().count("chromium/x-data-transfer-endpoint"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string text_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &text_result));
  EXPECT_EQ("Test data", text_result);

  // Attempt to retrieve encoded clipboard source data transfer endpoint.
  // Nothing should be returned.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("chromium/x-data-transfer-endpoint",
                     std::move(write_pipe));
  std::string dte_json_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &dte_json_result));
  EXPECT_EQ("", dte_json_result);
}

// See crbug.com/1339344
TEST_F(DataOfferTest, SetClipboardDataOfferDteToLacrosSourceChanged) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kLacros);
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.SetDataSource(std::make_unique<ui::DataTransferEndpoint>(
        GURL("https://www.google.com")));
    writer.WriteText(u"Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(4u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));
  EXPECT_EQ(1u,
            delegate.mime_types().count("chromium/x-data-transfer-endpoint"));

  // Clipboard data changed
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Data changed with no source");
  }

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string text_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &text_result));
  EXPECT_EQ("Data changed with no source", text_result);

  // Retrieve encoded clipboard source data transfer endpoint.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(
      data_offer.Receive("chromium/x-data-transfer-endpoint",
                         std::move(write_pipe));
      ,
      "Check failed: data_src. Clipboard source DataTransferEndpoint has "
      "changed after initial MIME advertising. If you see this please file a "
      "bug and contact the chromeos-dlp team.");
#else
  data_offer.Receive("chromium/x-data-transfer-endpoint",
                     std::move(write_pipe));
  std::string dte_json_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &dte_json_result));
  EXPECT_EQ("", dte_json_result);
#endif
}

TEST_F(DataOfferTest, SetDropDataOfferDteToLacros) {
  base::flat_set<DndAction> source_actions;
  source_actions.insert(DndAction::kCopy);
  source_actions.insert(DndAction::kMove);

  ui::OSExchangeData data;
  data.SetString(std::u16string(u"Test data"));
  data.SetSource(std::make_unique<ui::DataTransferEndpoint>(
      GURL("https://www.google.com")));

  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kLacros);

  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);
  data_offer.SetSourceActions(source_actions);
  data_offer.SetActions(base::flat_set<DndAction>(), DndAction::kMove);

  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u,
            delegate.mime_types().count("chromium/x-data-transfer-endpoint"));
  EXPECT_EQ(2u, delegate.source_actions().size());
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kCopy));
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kMove));
  EXPECT_EQ(DndAction::kMove, delegate.dnd_action());

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string text_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &text_result));
  EXPECT_EQ("Test data", text_result);

  // Retrieve encoded drag source data transfer endpoint.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("chromium/x-data-transfer-endpoint",
                     std::move(write_pipe));
  std::string dte_json_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &dte_json_result));
  EXPECT_EQ(
      "{\"endpoint_type\":\"url\","
      "\"off_the_record\":false,"
      "\"url\":\"https://www.google.com/\"}",
      dte_json_result);
}

TEST_F(DataOfferTest, SetDropDataDoNotOfferDteToNonLacros) {
  base::flat_set<DndAction> source_actions;
  source_actions.insert(DndAction::kCopy);
  source_actions.insert(DndAction::kMove);

  ui::OSExchangeData data;
  data.SetString(std::u16string(u"Test data"));
  data.SetSource(std::make_unique<ui::DataTransferEndpoint>(
      GURL("https://www.google.com")));

  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  EXPECT_EQ(0u, delegate.mime_types().size());
  EXPECT_EQ(0u, delegate.source_actions().size());
  EXPECT_EQ(DndAction::kNone, delegate.dnd_action());

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kCrostini);

  data_offer.SetDropData(&data_exchange_delegate, nullptr, data);
  data_offer.SetSourceActions(source_actions);
  data_offer.SetActions(base::flat_set<DndAction>(), DndAction::kMove);

  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(0u,
            delegate.mime_types().count("chromium/x-data-transfer-endpoint"));
  EXPECT_EQ(2u, delegate.source_actions().size());
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kCopy));
  EXPECT_EQ(1u, delegate.source_actions().count(DndAction::kMove));
  EXPECT_EQ(DndAction::kMove, delegate.dnd_action());

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string text_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &text_result));
  EXPECT_EQ("Test data", text_result);

  // Attempt to retrieve encoded drag source data transfer endpoint.
  // Nothing should be returned.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("chromium/x-data-transfer-endpoint",
                     std::move(write_pipe));
  std::string dte_json_result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &dte_json_result));
  EXPECT_EQ("", dte_json_result);
}

TEST_F(DataOfferTest, SetClipboardDataHTML) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(u"Test data", "");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(3u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/html;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/html"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/html;charset=utf-8", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/html;charset=utf-16", std::move(write_pipe));
  std::u16string result16;
  ASSERT_TRUE(ReadString16(std::move(read_pipe), &result16));
  EXPECT_EQ("Test data", base::UTF16ToUTF8(result16));

  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/html", std::move(write_pipe));
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);
}

TEST_F(DataOfferTest, SetClipboardDataRTF) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteRTF("Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

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

TEST_F(DataOfferTest, SetClipboardDataImage) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  TestDataExchangeDelegate data_exchange_delegate;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteImage(image);
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("image/png"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  base::ScopedFD read_pipe2;
  base::ScopedFD write_pipe2;
  std::string result;

  // Call Receive() twice in quick succession. Requires RunUntilIdle() since
  // processing is done on worker thread.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  ASSERT_TRUE(base::CreatePipe(&read_pipe2, &write_pipe2));
  data_offer.Receive("image/png", std::move(write_pipe));
  data_offer.Receive("image/png", std::move(write_pipe2));
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  SkBitmap decoded;
  ASSERT_TRUE(gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(result.data()), result.size(),
      &decoded));
  EXPECT_TRUE(cc::MatchesBitmap(image, decoded, cc::ExactPixelComparator()));
  std::string good = result;
  ASSERT_TRUE(ReadString(std::move(read_pipe2), &result));
  EXPECT_EQ(good, result);

  // Receive() should now return immediately with result from cache.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("image/png", std::move(write_pipe));
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ(good, result);
}

TEST_F(DataOfferTest, SetClipboardDataFilenames) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames("file:///test/path");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(1u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/uri-list"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));

  data_offer.Receive("text/uri-list", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("file:///test/path", result);
}

TEST_F(DataOfferTest, AcceptWithNull) {
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);
  data_offer.Accept(nullptr);
}

TEST_F(DataOfferTest, SetClipboardDataWithTransferPolicy) {
  TestDataTransferPolicyController policy_controller;
  TestDataOfferDelegate delegate;
  DataOffer data_offer(&delegate);

  TestDataExchangeDelegate data_exchange_delegate;
  data_exchange_delegate.set_endpoint_type(ui::EndpointType::kCrostini);
  {
    ui::ScopedClipboardWriter writer(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::DataTransferEndpoint>(ui::EndpointType::kArc));
    writer.WriteText(u"Test data");
  }

  auto* window = CreateTestWindowInShellWithBounds(gfx::Rect());
  data_offer.SetClipboardData(
      &data_exchange_delegate, *ui::Clipboard::GetForCurrentThread(),
      data_exchange_delegate.GetDataTransferEndpointType(window));

  EXPECT_EQ(3u, delegate.mime_types().size());
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-8"));
  EXPECT_EQ(1u, delegate.mime_types().count("text/plain;charset=utf-16"));
  EXPECT_EQ(1u, delegate.mime_types().count("UTF8_STRING"));

  base::ScopedFD read_pipe;
  base::ScopedFD write_pipe;

  // Read as utf-8.
  ASSERT_TRUE(base::CreatePipe(&read_pipe, &write_pipe));
  data_offer.Receive("text/plain;charset=utf-8", std::move(write_pipe));
  std::string result;
  ASSERT_TRUE(ReadString(std::move(read_pipe), &result));
  EXPECT_EQ("Test data", result);

  EXPECT_EQ(ui::EndpointType::kArc, policy_controller.last_src_type());
  EXPECT_EQ(ui::EndpointType::kCrostini, policy_controller.last_dst_type());
}

}  // namespace
}  // namespace exo
