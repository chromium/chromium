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
      ui::ClipboardFormatType::CustomPlatformType(kFormatString));
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

void OnReceiveTextFromClipboard(const std::string& charset,
                                DataOffer::SendDataCallback callback,
                                std::u16string text) {
  std::move(callback).Run(EncodeAsRefCountedString(text, charset));
}

void ReadTextFromClipboard(const std::string& charset,
                           const ui::DataTransferEndpoint data_dst,
                           DataOffer::SendDataCallback callback) {
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnReceiveTextFromClipboard, charset,
                     std::move(callback)));
}

void OnReceiveHTMLFromClipboard(const std::string& charset,
                                DataOffer::SendDataCallback callback,
                                std::u16string text,
                                GURL src_url,
                                uint32_t fragment_start,
                                uint32_t fragment_end) {
  std::move(callback).Run(EncodeAsRefCountedString(text, charset));
}

void ReadHTMLFromClipboard(const std::string& charset,
                           const ui::DataTransferEndpoint data_dst,
                           DataOffer::SendDataCallback callback) {
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnReceiveHTMLFromClipboard, charset,
                     std::move(callback)));
}

void OnReceiveRTFFromClipboard(DataOffer::SendDataCallback callback,
                               std::string text) {
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(text)));
}

void ReadRTFFromClipboard(const ui::DataTransferEndpoint data_dst,
                          DataOffer::SendDataCallback callback) {
  ui::Clipboard::GetForCurrentThread()->ReadRTF(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnReceiveRTFFromClipboard, std::move(callback)));
}

void OnReceiveFilenamesFromClipboard(ui::EndpointType endpoint_type,
                                     base::WeakPtr<DataOfferDelegate> delegate,
                                     DataOffer::SendDataCallback callback,
                                     std::vector<ui::FileInfo> filenames) {
  if (filenames.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!delegate) {
    std::move(callback).Run(nullptr);
    return;
  }
  delegate->GetSecurityDelegate()->SendFileInfo(
      endpoint_type, std::move(filenames), std::move(callback));
}

void ReadFilenamesFromClipboard(ui::EndpointType endpoint_type,
                                base::WeakPtr<DataOfferDelegate> delegate,
                                const ui::DataTransferEndpoint data_dst,
                                DataOffer::SendDataCallback callback) {
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnReceiveFilenamesFromClipboard, endpoint_type, delegate,
                     std::move(callback)));
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
      ui::ClipboardBuffer::kCopyPaste, data_dst,
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
    : delegate_(delegate->GetWeakPtr()),
      dnd_action_(DndAction::kNone),
      finished_(false) {}

DataOffer::~DataOffer() {
  if (delegate_) {
    delegate_->OnDataOfferDestroying(this);
  }
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
  if (delegate_) {
    delegate_->OnAction(preferred_action);
  }
}

void DataOffer::SetSourceActions(
    const base::flat_set<DndAction>& source_actions) {
  source_actions_ = source_actions;
  if (delegate_) {
    delegate_->OnSourceActions(source_actions);
  }
}

void DataOffer::SetDropData(DataExchangeDelegate* data_exchange_delegate,
                            aura::Window* target,
                            const ui::OSExchangeData& data) {
  if (!delegate_) {
    return;
  }
  DCHECK_EQ(0u, data_callbacks_.size());

  ui::EndpointType endpoint_type =
      data_exchange_delegate->GetDataTransferEndpointType(target);

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
    auto callback = base::BindOnce(
        [](base::WeakPtr<DataOfferDelegate> delegate,
           ui::EndpointType endpoint_type, std::vector<ui::FileInfo> filenames,
           DataOffer::SendDataCallback callback) {
          if (!delegate) {
            std::move(callback).Run(nullptr);
            return;
          }
          delegate->GetSecurityDelegate()->SendFileInfo(
              endpoint_type, std::move(filenames), std::move(callback));
        },
        delegate_, endpoint_type, std::move(filenames));
    data_callbacks_.emplace(uri_list_mime_type, std::move(callback));
    delegate_->OnOffer(uri_list_mime_type);
    return;
  }

  if (std::optional<base::Pickle> pickle =
          data.GetPickledData(GetClipboardFormatType());
      pickle.has_value() &&
      data_exchange_delegate->HasUrlsInPickle(pickle.value())) {
    auto callback = base::BindOnce(
        [](base::WeakPtr<DataOfferDelegate> delegate,
           ui::EndpointType endpoint_type, base::Pickle pickle,
           DataOffer::SendDataCallback callback) {
          if (!delegate) {
            std::move(callback).Run(nullptr);
            return;
          }
          delegate->GetSecurityDelegate()->SendPickle(endpoint_type, pickle,
                                                      std::move(callback));
        },
        delegate_, endpoint_type, pickle.value());
    data_callbacks_.emplace(uri_list_mime_type, std::move(callback));
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
        [](scoped_refptr<base::RefCountedMemory> contents,
           DataOffer::SendDataCallback callback) {
          std::move(callback).Run(std::move(contents));
        },
        base::MakeRefCounted<base::RefCountedBytes>(
            std::move(file_contents->file_contents)));

    data_callbacks_.emplace(mime_type, std::move(callback));
    delegate_->OnOffer(mime_type);
  }

  if (std::optional<std::u16string> string_content = data.GetString();
      string_content.has_value()) {
    const std::string utf8_mime_type = std::string(ui::kMimeTypeUtf8PlainText);
    data_callbacks_.emplace(
        utf8_mime_type, AsyncEncodeAsRefCountedString(*string_content, kUTF8));
    delegate_->OnOffer(utf8_mime_type);
    const std::string utf16_mime_type = std::string(kTextMimeTypeUtf16);
    data_callbacks_.emplace(utf16_mime_type, AsyncEncodeAsRefCountedString(
                                                 *string_content, kUTF16));
    delegate_->OnOffer(utf16_mime_type);
    const std::string text_plain_mime_type =
        std::string(ui::kMimeTypePlainText);
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
    const std::string utf8_html_mime_type = std::string(ui::kMimeTypeUtf8Html);
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

  data.GetAllAvailableFormats(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(
          [](base::WeakPtr<DataOffer> data_offer,
             DataExchangeDelegate* data_exchange_delegate,
             const ui::DataTransferEndpoint& data_dst,
             ui::EndpointType endpoint_type,
             base::flat_set<ui::ClipboardFormatType> formats) {
            if (!data_offer || !data_offer->delegate_) {
              return;
            }
            if (formats.contains(ui::ClipboardFormatType::PlainTextType())) {
              auto utf8_callback = base::BindRepeating(
                  &ReadTextFromClipboard, std::string(kUTF8), data_dst);
              data_offer->delegate_->OnOffer(
                  std::string(ui::kMimeTypeUtf8PlainText));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeUtf8PlainText), utf8_callback);
              data_offer->delegate_->OnOffer(
                  std::string(ui::kMimeTypeLinuxUtf8String));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeLinuxUtf8String), utf8_callback);
              data_offer->delegate_->OnOffer(std::string(kTextMimeTypeUtf16));
              data_offer->data_callbacks_.emplace(
                  std::string(kTextMimeTypeUtf16),
                  base::BindOnce(&ReadTextFromClipboard, std::string(kUTF16),
                                 data_dst));
            }
            if (formats.contains(ui::ClipboardFormatType::HtmlType())) {
              data_offer->delegate_->OnOffer(
                  std::string(ui::kMimeTypeUtf8Html));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeUtf8Html),
                  base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF8),
                                 data_dst));
              data_offer->delegate_->OnOffer(
                  std::string(kTextHtmlMimeTypeUtf16));
              data_offer->data_callbacks_.emplace(
                  std::string(kTextHtmlMimeTypeUtf16),
                  base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF16),
                                 data_dst));
              data_offer->delegate_->OnOffer(std::string(ui::kMimeTypeHtml));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeHtml),
                  base::BindOnce(&ReadHTMLFromClipboard, std::string(kUTF8),
                                 data_dst));
            }
            if (formats.contains(ui::ClipboardFormatType::RtfType())) {
              data_offer->delegate_->OnOffer(std::string(ui::kMimeTypeRtf));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeRtf),
                  base::BindOnce(&ReadRTFFromClipboard, data_dst));
            }
            if (formats.contains(ui::ClipboardFormatType::BitmapType()) ||
                formats.contains(ui::ClipboardFormatType::PngType())) {
              data_offer->delegate_->OnOffer(std::string(ui::kMimeTypePng));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypePng),
                  base::BindOnce(&ReadPNGFromClipboard, data_dst));
            }
            if (formats.contains(ui::ClipboardFormatType::FilenamesType())) {
              data_offer->delegate_->OnOffer(std::string(ui::kMimeTypeUriList));
              data_offer->data_callbacks_.emplace(
                  std::string(ui::kMimeTypeUriList),
                  base::BindOnce(&ReadFilenamesFromClipboard, endpoint_type,
                                 data_offer->delegate_, data_dst));
            }
          },
          weak_ptr_factory_.GetWeakPtr(), data_exchange_delegate, data_dst,
          endpoint_type));
}

void DataOffer::OnDataReady(const std::string& mime_type,
                            base::ScopedFD fd,
                            scoped_refptr<base::RefCountedMemory> data) {
  // Update cache from nullptr to data.
  const auto cache_it = data_cache_.find(mime_type);
  CHECK(cache_it != data_cache_.end());
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
