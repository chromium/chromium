// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/exo/data_device.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/security_delegate.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

constexpr char kTextMimeTypeUtf16[] = "text/plain;charset=utf-16";
constexpr char kTextHtmlMimeTypeUtf16[] = "text/html;charset=utf-16";

constexpr char kUTF8[] = "utf8";
constexpr char kUTF16[] = "utf16";

void WriteFileDescriptorOnWorkerThread(
    base::ScopedFD fd,
    scoped_refptr<base::RefCountedMemory> memory) {
  if (!base::WriteFileDescriptor(fd.get(), *memory))
    DLOG(ERROR) << "Failed to write drop data";
}

void WriteFileDescriptor(base::ScopedFD fd,
                         scoped_refptr<base::RefCountedMemory> memory) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteFileDescriptorOnWorkerThread, std::move(fd),
                     std::move(memory)));
}

ui::ClipboardFormatType GetClipboardFormatType() {
  static const char kFormatString[] = "chromium/x-file-system-files";
  static base::NoDestructor<ui::ClipboardFormatType> format_type(
      ui::ClipboardFormatType::GetType(kFormatString));
  return *format_type;
}

scoped_refptr<base::RefCountedString> EncodeAsRefCountedString(
    const std::u16string& text,
    const std::string& charset) {
  std::string encoded_text;
  base::UTF16ToCodepage(text, charset.c_str(),
                        base::OnStringConversionError::SUBSTITUTE,
                        &encoded_text);
  return base::MakeRefCounted<base::RefCountedString>(std::move(encoded_text));
}

DataOffer::AsyncSendDataCallback AsyncEncodeAsRefCountedString(
    const std::u16string& text,
    const std::string& charset) {
  return base::BindOnce(
      [](const std::u16string& text, const std::string& charset,
         DataOffer::SendDataCallback callback) {
        std::move(callback).Run(EncodeAsRefCountedString(text, charset));
      },
      text, charset);
}

void ReadDataTransferEndpointFromClipboard(
    const std::string& charset,
    const ui::DataTransferEndpoint data_dst,
    DataOffer::SendDataCallback callback) {
  std::optional<ui::DataTransferEndpoint> data_src =
      ui::Clipboard::GetForCurrentThread()->GetSource(
          ui::ClipboardBuffer::kCopyPaste);

  std::u16string encoded_endpoint;
  if (data_src) {
    encoded_endpoint =
        base::UTF8ToUTF16(ui::ConvertDataTransferEndpointToJson(*data_src));
  } else {
    DCHECK(data_src) << "Clipboard source DataTransferEndpoint has changed "
                        "after initial MIME advertising. If you see this "
                        "please file a bug and contact the chromeos-dlp team.";
  }

  std::move(callback).Run(EncodeAsRefCountedString(encoded_endpoint, charset));
}

void ReadTextFromClipboard(const std::string& charset,
                           const ui::DataTransferEndpoint data_dst,
                           DataOffer::SendDataCallback callback) {
  std::u16string text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &text);
  std::move(callback).Run(EncodeAsRefCountedString(text, charset));
}

void ReadHTMLFromClipboard(const std::string& charset,
                           const ui::DataTransferEndpoint data_dst,
                           DataOffer::SendDataCallback callback) {
  std::u16string text;
  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &text, &url, &start, &end);
  std::move(callback).Run(EncodeAsRefCountedString(text, charset));
}

void ReadRTFFromClipboard(const ui::DataTransferEndpoint data_dst,
                          DataOffer::SendDataCallback callback) {
  std::string text;
  ui::Clipboard::GetForCurrentThread()->ReadRTF(ui::ClipboardBuffer::kCopyPaste,
                                                &data_dst, &text);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(text)));
}

void OnReceivePNGFromClipboard(DataOffer::SendDataCallback callback,
                               const std::vector<uint8_t>& png) {
  scoped_refptr<base::RefCountedMemory> rc_mem =
      base::MakeRefCounted<base::RefCountedBytes>(png);
  std::move(callback).Run(std::move(rc_mem));
}

void ReadPNGFromClipboard(const ui::DataTransferEndpoint data_dst,
                          DataOffer::SendDataCallback callback) {
  ui::Clipboard::GetForCurrentThread()->ReadPng(
      ui::ClipboardBuffer::kCopyPaste, &data_dst,
      base::BindOnce(&OnReceivePNGFromClipboard, std::move(callback)));
}

}  // namespace

ScopedDataOffer::ScopedDataOffer(DataOffer* data_offer,
                                 DataOfferObserver* observer)
    : data_offer_(data_offer), observer_(observer) {
  data_offer_->AddObserver(observer_);
}

ScopedDataOffer::~ScopedDataOffer() {
  data_offer_->RemoveObserver(observer_);
}

DataOffer::DataOffer(DataOfferDelegate* delegate)
    : delegate_(delegate), dnd_action_(DndAction::kNone), finished_(false) {}

DataOffer::~DataOffer() {
  delegate_->OnDataOfferDestroying(this);
  for (DataOfferObserver& observer : observers_) {
    observer.OnDataOfferDestroying(this);
  }
}

void DataOffer::AddObserver(DataOfferObserver* observer) {
  observers_.AddObserver(observer);
}

void DataOffer::RemoveObserver(DataOfferObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataOffer::Accept(const std::string* mime_type) {}

void DataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  const auto callbacks_it = data_callbacks_.find(mime_type);
  if (callbacks_it != data_callbacks_.end()) {
    // Set cache with empty data to indicate in process.
    DCHECK(data_cache_.count(mime_type) == 0);
    data_cache_.emplace(mime_type, nullptr);
    std::move(callbacks_it->second)
        .Run(base::BindOnce(&DataOffer::OnDataReady,
                            weak_ptr_factory_.GetWeakPtr(), mime_type,
                            std::move(fd)));
    data_callbacks_.erase(callbacks_it);
    return;
  }

  const auto cache_it = data_cache_.find(mime_type);
  if (cache_it == data_cache_.end()) {
    DLOG(ERROR) << "Unexpected mime type is requested " << mime_type;
    return;
  }

  if (cache_it->second) {
    WriteFileDescriptor(std::move(fd), cache_it->second);
  } else {
    // Data bytes for this mime type are being processed currently.
    pending_receive_requests_.push_back(
        std::make_pair(mime_type, std::move(fd)));
  }
}

void DataOffer::Finish() {
  finished_ = true;
}

void DataOffer::SetActions(const base::flat_set<DndAction>& dnd_actions,
                           DndAction preferred_action) {
  dnd_action_ = preferred_action;
  delegate_->OnAction(preferred_action);
}

void DataOffer::SetSourceActions(
    const base::flat_set<DndAction>& source_actions) {
  source_actions_ = source_actions;
  delegate_->OnSourceActions(source_actions);
}

void DataOffer::SetDropData(DataExchangeDelegate* data_exchange_delegate,
                            aura::Window* target,
                            const ui::OSExchangeData& data) {
  DCHECK_EQ(0u, data_callbacks_.size());

  ui::EndpointType endpoint_type =
      data_exchange_delegate->GetDataTransferEndpointType(target);

  // Drag & Drop source metadata (if any) is synced between Ash and Lacros by
  // encoding the metadata into a custom MIME type.
  if (endpoint_type == ui::EndpointType::kLacros && data.GetSource()) {
    std::u16string encoded_endpoint = base::UTF8ToUTF16(
        ui::ConvertDataTransferEndpointToJson(*data.GetSource()));
    data_callbacks_.emplace(
        ui::kMimeTypeDataTransferEndpoint,
        AsyncEncodeAsRefCountedString(encoded_endpoint, kUTF8));
    delegate_->OnOffer(ui::kMimeTypeDataTransferEndpoint);
  }

  const std::string uri_list_mime_type =
      data_exchange_delegate->GetMimeTypeForUriList(endpoint_type);
  // We accept the filenames pickle from FilesApp, or
  // OSExchangeData::GetFilenames().
  std::vector<ui::FileInfo> filenames;
  if (std::optional<base::Pickle> pickle = data.GetPickledData(
          ui::ClipboardFormatType::DataTransferCustomType());
      pickle.has_value()) {
    filenames = data_exchange_delegate->ParseFileSystemSources(data.GetSource(),
                                                               pickle.value());
  }

  if (filenames.empty() && data.HasFile()) {
    if (std::optional<std::vector<ui::FileInfo>> file_info =
            data.GetFilenames();
        file_info.has_value()) {
      std::ranges::move(file_info.value(), std::back_inserter(filenames));
    }
  }

  if (!filenames.empty()) {
    data_callbacks_.emplace(
        uri_list_mime_type,
        base::BindOnce(&SecurityDelegate::SendFileInfo,
                       base::Unretained(delegate_->GetSecurityDelegate()),
                       endpoint_type, std::move(filenames)));
    delegate_->OnOffer(uri_list_mime_type);
    return;
  }

  if (std::optional<base::Pickle> pickle =
          data.GetPickledData(GetClipboardFormatType());
      pickle.has_value() &&
      data_exchange_delegate->HasUrlsInPickle(pickle.value())) {
    data_callbacks_.emplace(
        uri_list_mime_type,
        base::BindOnce(&SecurityDelegate::SendPickle,
                       base::Unretained(delegate_->GetSecurityDelegate()),
                       endpoint_type, pickle.value()));
    delegate_->OnOffer(uri_list_mime_type);
    return;
  }

  if (std::optional<ui::OSExchangeDataProvider::FileContentsInfo>
          file_contents = data.provider().GetFileContents();
      file_contents.has_value()) {
    std::string filename = file_contents->filename.value();
    base::ReplaceChars(filename, "\\", "\\\\", &filename);
    base::ReplaceChars(filename, "\"", "\\\"", &filename);
    const std::string mime_type =
        base::StrCat({"application/octet-stream;name=\"", filename, "\""});
    auto callback = base::BindOnce(
        [](scoped_refptr<base::RefCountedString> contents,
           DataOffer::SendDataCallback callback) {
          std::move(callback).Run(std::move(contents));
        },
        base::MakeRefCounted<base::RefCountedString>(
            std::move(file_contents->file_contents)));

    data_callbacks_.emplace(mime_type, std::move(callback));
    delegate_->OnOffer(mime_type);
  }

  if (std::optional<std::u16string> string_content = data.GetString();
      string_content.has_value()) {
    const std::string utf8_mime_type = std::string(ui::kMimeTypeTextUtf8);
    data_callbacks_.emplace(
        utf8_mime_type, AsyncEncodeAsRefCountedString(*string_content, kUTF8));
    delegate_->OnOffer(utf8_mime_type);
    const std::string utf16_mime_type = std::string(kTextMimeTypeUtf16);
    data_callbacks_.emplace(utf16_mime_type, AsyncEncodeAsRefCountedString(
                                                 *string_content, kUTF16));
    delegate_->OnOffer(utf16_mime_type);
    const std::string text_plain_mime_type = std::string(ui::kMimeTypeText);
    // The MIME type standard says that new text/ subtypes should default to a
    // UTF-8 encoding, but that old ones, including text/plain, keep ASCII as
    // the default. Nonetheless, we use UTF8 here because it is a superset of
    // ASCII and the defacto standard text encoding.
    data_callbacks_.emplace(text_plain_mime_type, AsyncEncodeAsRefCountedString(
                                                      *string_content, kUTF8));
    delegate_->OnOffer(text_plain_mime_type);
  }

  if (std::optional<ui::OSExchangeData::HtmlInfo> html_content = data.GetHtml();
      html_content.has_value()) {
    const std::string utf8_html_mime_type = std::string(ui::kMimeTypeHTMLUtf8);
    data_callbacks_.emplace(
        utf8_html_mime_type,
        AsyncEncodeAsRefCountedString(html_content->html, kUTF8));
    delegate_->OnOffer(utf8_html_mime_type);

    const std::string utf16_html_mime_type =
        std::string(kTextHtmlMimeTypeUtf16);
    data_callbacks_.emplace(
        utf16_html_mime_type,
        AsyncEncodeAsRefCountedString(html_content->html, kUTF16));
    delegate_->OnOffer(utf16_html_mime_type);
  }
}

void DataOffer::SetClipboardData(DataExchangeDelegate* data_exchange_delegate,
                                 const ui::Clipboard& data,
                                 ui::EndpointType endpoint_type) {
  DCHECK_EQ(0u, data_callbacks_.size());
  const ui::DataTransferEndpoint data_dst(endpoint_type);

  // Clipboard source metadata (if any) is synced between Ash and Lacros by
  // encoding the metadata into a custom MIME type.
  if (endpoint_type == ui::EndpointType::kLacros &&
      data.GetSource(ui::ClipboardBuffer::kCopyPaste)) {
    delegate_->OnOffer(std::string(ui::kMimeTypeDataTransferEndpoint));
    data_callbacks_.emplace(
        std::string(ui::kMimeTypeDataTransferEndpoint),
        base::BindOnce(&ReadDataTransferEndpointFromClipboard,
                       std::string(kUTF8), data_dst));
  }

  if (data.IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                             ui::ClipboardBuffer::kCopyPaste, &data_dst)) {
    auto utf8_callback = base::BindRepeating(&ReadTextFromClipboard,
                                             std::string(kUTF8), data_dst);
    delegate_->OnOffer(std::string(ui::kMimeTypeTextUtf8));
    data_callbacks_.emplace(std::string(ui::kMimeTypeTextUtf8), utf8_callback);
    delegate_->OnOffer(std::string(ui::kMimeTypeLinuxUtf8String));
    data_callbacks_.emplace(std::string(ui::kMimeTypeLinuxUtf8String),
                            utf8_callback);
    delegate_->OnOffer(std::string(kTextMimeTypeUtf16));
    data_callbacks_.emplace(
        std::string(kTextMimeTypeUtf16),
        base::BindOnce(&ReadTextFromClipboard, std::string(kUTF16), data_dst));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::HtmlType(),
                             ui::ClipboardBuffer::kCopyPaste, &data_dst)) {
    delegate_->OnOffer(std::string(ui::kMimeTypeHTMLUtf8));
    data_callbacks_.emplace(
        std::string(ui::kMimeTypeHTMLUtf8),
        base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF8), data_dst));
    delegate_->OnOffer(std::string(kTextHtmlMimeTypeUtf16));
    data_callbacks_.emplace(
        std::string(kTextHtmlMimeTypeUtf16),
        base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF16), data_dst));
    delegate_->OnOffer(std::string(ui::kMimeTypeHTML));
    data_callbacks_.emplace(
        std::string(ui::kMimeTypeHTML),
        base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF8), data_dst));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::RtfType(),
                             ui::ClipboardBuffer::kCopyPaste, &data_dst)) {
    delegate_->OnOffer(std::string(ui::kMimeTypeRTF));
    data_callbacks_.emplace(std::string(ui::kMimeTypeRTF),
                            base::BindOnce(&ReadRTFFromClipboard, data_dst));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::BitmapType(),
                             ui::ClipboardBuffer::kCopyPaste, &data_dst)) {
    delegate_->OnOffer(std::string(ui::kMimeTypePNG));
    data_callbacks_.emplace(std::string(ui::kMimeTypePNG),
                            base::BindOnce(&ReadPNGFromClipboard, data_dst));
  }

  // For clipboard, FilesApp filenames pickle is already converted to files
  // in VolumeManager::OnClipboardDataChanged().
  std::vector<ui::FileInfo> filenames;
  if (data.IsFormatAvailable(ui::ClipboardFormatType::FilenamesType(),
                             ui::ClipboardBuffer::kCopyPaste, &data_dst)) {
    data.ReadFilenames(ui::ClipboardBuffer::kCopyPaste, &data_dst, &filenames);
  }
  if (!filenames.empty()) {
    delegate_->OnOffer(std::string(ui::kMimeTypeURIList));
    data_callbacks_.emplace(
        std::string(ui::kMimeTypeURIList),
        base::BindOnce(&SecurityDelegate::SendFileInfo,
                       base::Unretained(delegate_->GetSecurityDelegate()),
                       endpoint_type, std::move(filenames)));
  }
}

void DataOffer::OnDataReady(const std::string& mime_type,
                            base::ScopedFD fd,
                            scoped_refptr<base::RefCountedMemory> data) {
  // Update cache from nullptr to data.
  const auto cache_it = data_cache_.find(mime_type);
  CHECK(cache_it != data_cache_.end(), base::NotFatalUntil::M130);
  DCHECK(!cache_it->second);
  data_cache_.erase(cache_it);
  data_cache_.emplace(mime_type, data);

  WriteFileDescriptor(std::move(fd), data);

  // Process pending receive requests for this mime type, if there are any.
  auto it = pending_receive_requests_.begin();
  while (it != pending_receive_requests_.end()) {
    if (it->first == mime_type) {
      WriteFileDescriptor(std::move(it->second), data);
      it = pending_receive_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace exo
