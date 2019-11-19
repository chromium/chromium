// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/file_helper.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

constexpr char kTextMimeTypeUtf8[] = "text/plain;charset=utf-8";
constexpr char kUtf8String[] = "UTF8_STRING";
constexpr char kTextMimeTypeUtf16[] = "text/plain;charset=utf-16";
constexpr char kTextHtmlMimeTypeUtf8[] = "text/html;charset=utf-8";
constexpr char kTextHtmlMimeTypeUtf16[] = "text/html;charset=utf-16";
constexpr char kTextRtfMimeType[] = "text/rtf";
constexpr char kImagePngMimeType[] = "image/png";
constexpr char kUriListSeparator[] = "\r\n";

constexpr char kUTF8[] = "utf8";
constexpr char kUTF16[] = "utf16";

class RefCountedString16 : public base::RefCountedMemory {
 public:
  static scoped_refptr<RefCountedString16> TakeString(
      base::string16&& to_destroy) {
    scoped_refptr<RefCountedString16> self(new RefCountedString16);
    to_destroy.swap(self->data_);
    return self;
  }

  // Overridden from base::RefCountedMemory:
  const unsigned char* front() const override {
    return reinterpret_cast<const unsigned char*>(data_.data());
  }
  size_t size() const override { return data_.size() * sizeof(base::char16); }

 protected:
  ~RefCountedString16() override {}

 private:
  base::string16 data_;
};

void WriteFileDescriptorOnWorkerThread(
    base::ScopedFD fd,
    scoped_refptr<base::RefCountedMemory> memory) {
  if (!base::WriteFileDescriptor(fd.get(),
                                 reinterpret_cast<const char*>(memory->front()),
                                 memory->size()))
    DLOG(ERROR) << "Failed to write drop data";
}

void WriteFileDescriptor(base::ScopedFD fd,
                         scoped_refptr<base::RefCountedMemory> memory) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteFileDescriptorOnWorkerThread, std::move(fd),
                     std::move(memory)));
}

// Gets a comma-separated list of urls extracted from |data|->file.
bool GetUrlListFromDataFile(FileHelper* file_helper,
                            const ui::OSExchangeData& data,
                            base::string16* url_list_string) {
  if (!data.HasFile())
    return false;
  std::vector<ui::FileInfo> files;
  if (data.GetFilenames(&files)) {
    for (const auto& info : files) {
      GURL url;
      // TODO(niwa): Need to fill the correct app_id.
      if (file_helper->GetUrlFromPath(/* app_id */ "", info.path, &url)) {
        if (!url_list_string->empty())
          *url_list_string += base::UTF8ToUTF16(kUriListSeparator);
        *url_list_string += base::UTF8ToUTF16(url.spec());
      }
    }
  }
  return !url_list_string->empty();
}

ui::ClipboardFormatType GetClipboardFormatType() {
  static const char kFormatString[] = "chromium/x-file-system-files";
  static base::NoDestructor<ui::ClipboardFormatType> format_type(
      ui::ClipboardFormatType::GetType(kFormatString));
  return *format_type;
}

scoped_refptr<base::RefCountedString> EncodeAsRefCountedString(
    const base::string16& text,
    const std::string& charset) {
  std::string encoded_text;
  base::UTF16ToCodepage(text, charset.c_str(),
                        base::OnStringConversionError::SUBSTITUTE,
                        &encoded_text);
  return base::RefCountedString::TakeString(&encoded_text);
}

void ReadTextFromClipboard(const std::string& charset, base::ScopedFD fd) {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &text);
  WriteFileDescriptor(std::move(fd), EncodeAsRefCountedString(text, charset));
}

void ReadHTMLFromClipboard(const std::string& charset, base::ScopedFD fd) {
  base::string16 text;
  std::string url;
  uint32_t start, end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, &text, &url, &start, &end);
  WriteFileDescriptor(std::move(fd), EncodeAsRefCountedString(text, charset));
}

void ReadRTFFromClipboard(base::ScopedFD fd) {
  std::string text;
  ui::Clipboard::GetForCurrentThread()->ReadRTF(ui::ClipboardBuffer::kCopyPaste,
                                                &text);
  WriteFileDescriptor(std::move(fd), base::RefCountedString::TakeString(&text));
}

void SendAsPNGOnWorkerThread(base::ScopedFD fd, const SkBitmap sk_bitmap) {
  SkDynamicMemoryWStream data_stream;
  if (SkEncodeImage(&data_stream, sk_bitmap.pixmap(),
                    SkEncodedImageFormat::kPNG, 100)) {
    std::vector<uint8_t> data(data_stream.bytesWritten());
    data_stream.copyToAndReset(data.data());
    WriteFileDescriptorOnWorkerThread(std::move(fd),
                                      base::RefCountedBytes::TakeVector(&data));
  } else {
    LOG(ERROR) << "Couldn't encode image as PNG";
  }
}

void ReadPNGFromClipboard(base::ScopedFD fd) {
  const SkBitmap sk_bitmap = ui::Clipboard::GetForCurrentThread()->ReadImage(
      ui::ClipboardBuffer::kCopyPaste);
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&SendAsPNGOnWorkerThread, std::move(fd),
                     std::move(sk_bitmap)));
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

DataOffer::DataOffer(DataOfferDelegate* delegate, Purpose purpose)
    : delegate_(delegate), purpose_(purpose) {}

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
  if (purpose_ == Purpose::COPY_PASTE) {
    const auto data_it = data_callbacks_.find(mime_type);
    if (data_it == data_callbacks_.end()) {
      DLOG(ERROR) << "Unexpected mime type is requested";
      return;
    }
    data_it->second.Run(std::move(fd));
  } else if (purpose_ == Purpose::DRAG_DROP) {
    const auto data_it = data_.find(mime_type);
    if (data_it == data_.end()) {
      DLOG(ERROR) << "Unexpected mime type is requested";
      return;
    }
    if (data_it->second) {
      WriteFileDescriptor(std::move(fd), data_it->second);
    } else {
      // Data bytes for this mime type are being processed currently.
      pending_receive_requests_.push_back(
          std::make_pair(mime_type, std::move(fd)));
    }
  } else {
    NOTREACHED();
  }
}

void DataOffer::Finish() {}

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

void DataOffer::SetDropData(FileHelper* file_helper,
                            const ui::OSExchangeData& data) {
  DCHECK_EQ(0u, data_.size());

  const std::string uri_list_mime_type = file_helper->GetMimeTypeForUriList();
  base::string16 url_list_string;
  if (GetUrlListFromDataFile(file_helper, data, &url_list_string)) {
    data_.emplace(uri_list_mime_type,
                  RefCountedString16::TakeString(std::move(url_list_string)));
    delegate_->OnOffer(uri_list_mime_type);
    return;
  }

  base::Pickle pickle;
  if (data.GetPickledData(GetClipboardFormatType(), &pickle) &&
      file_helper->HasUrlsInPickle(pickle)) {
    // Set nullptr as a temporary value for the mime type.
    // The value will be overriden in the callback below.
    data_.emplace(uri_list_mime_type, nullptr);
    // TODO(niwa): Need to fill the correct app_id.
    file_helper->GetUrlsFromPickle(
        /* app_id */ "", pickle,
        base::BindOnce(&DataOffer::OnPickledUrlsResolved,
                       weak_ptr_factory_.GetWeakPtr(), uri_list_mime_type));
    delegate_->OnOffer(uri_list_mime_type);
    return;
  }

  base::string16 string_content;
  if (data.HasString() && data.GetString(&string_content)) {
    const std::string utf8_mime_type = std::string(kTextMimeTypeUtf8);
    data_.emplace(utf8_mime_type,
                  EncodeAsRefCountedString(string_content, kUTF8));
    delegate_->OnOffer(utf8_mime_type);
    const std::string utf16_mime_type = std::string(kTextMimeTypeUtf16);
    data_.emplace(utf16_mime_type,
                  EncodeAsRefCountedString(string_content, kUTF16));
    delegate_->OnOffer(utf16_mime_type);
    const std::string text_plain_mime_type = std::string(ui::kMimeTypeText);
    // The MIME type standard says that new text/ subtypes should default to a
    // UTF-8 encoding, but that old ones, including text/plain, keep ASCII as
    // the default. Nonetheless, we use UTF8 here because it is a superset of
    // ASCII and the defacto standard text encoding.
    data_.emplace(text_plain_mime_type,
                  EncodeAsRefCountedString(string_content, kUTF8));
    delegate_->OnOffer(text_plain_mime_type);
  }

  base::string16 html_content;
  GURL url_content;
  if (data.HasHtml() && data.GetHtml(&html_content, &url_content)) {
    const std::string utf8_html_mime_type = std::string(kTextHtmlMimeTypeUtf8);
    data_.emplace(utf8_html_mime_type,
                  EncodeAsRefCountedString(html_content, kUTF8));
    delegate_->OnOffer(utf8_html_mime_type);

    const std::string utf16_html_mime_type =
        std::string(kTextHtmlMimeTypeUtf16);
    data_.emplace(utf16_html_mime_type,
                  EncodeAsRefCountedString(html_content, kUTF16));
    delegate_->OnOffer(utf16_html_mime_type);
  }
}

void DataOffer::SetClipboardData(FileHelper* file_helper,
                                 const ui::Clipboard& data) {
  DCHECK_EQ(0u, data_.size());
  if (data.IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextWType(),
                             ui::ClipboardBuffer::kCopyPaste)) {
    auto utf8_callback =
        base::BindRepeating(&ReadTextFromClipboard, std::string(kUTF8));
    delegate_->OnOffer(std::string(kTextMimeTypeUtf8));
    data_callbacks_.emplace(std::string(kTextMimeTypeUtf8), utf8_callback);
    delegate_->OnOffer(std::string(kUtf8String));
    data_callbacks_.emplace(std::string(kUtf8String), utf8_callback);
    delegate_->OnOffer(std::string(kTextMimeTypeUtf16));
    data_callbacks_.emplace(
        std::string(kTextMimeTypeUtf16),
        base::BindRepeating(&ReadTextFromClipboard, std::string(kUTF16)));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::GetHtmlType(),
                             ui::ClipboardBuffer::kCopyPaste)) {
    delegate_->OnOffer(std::string(kTextHtmlMimeTypeUtf8));
    data_callbacks_.emplace(
        std::string(kTextHtmlMimeTypeUtf8),
        base::BindRepeating(&ReadHTMLFromClipboard, std::string(kUTF8)));
    delegate_->OnOffer(std::string(kTextHtmlMimeTypeUtf16));
    data_callbacks_.emplace(
        std::string(kTextHtmlMimeTypeUtf16),
        base::BindRepeating(&ReadHTMLFromClipboard, std::string(kUTF16)));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::GetRtfType(),
                             ui::ClipboardBuffer::kCopyPaste)) {
    delegate_->OnOffer(std::string(kTextRtfMimeType));
    data_callbacks_.emplace(std::string(kTextRtfMimeType),
                            base::BindRepeating(&ReadRTFFromClipboard));
  }
  if (data.IsFormatAvailable(ui::ClipboardFormatType::GetBitmapType(),
                             ui::ClipboardBuffer::kCopyPaste)) {
    delegate_->OnOffer(std::string(kImagePngMimeType));
    data_callbacks_.emplace(std::string(kImagePngMimeType),
                            base::BindRepeating(&ReadPNGFromClipboard));
  }
}

void DataOffer::OnPickledUrlsResolved(const std::string& mime_type,
                                      const std::vector<GURL>& urls) {
  const auto data_it = data_.find(mime_type);
  DCHECK(data_it != data_.end());
  DCHECK(!data_it->second);  // nullptr should be set as a temporary value.
  data_.erase(data_it);

  base::string16 url_list_string;
  for (const GURL& url : urls) {
    if (!url.is_valid())
      continue;
    if (!url_list_string.empty())
      url_list_string += base::UTF8ToUTF16(kUriListSeparator);
    url_list_string += base::UTF8ToUTF16(url.spec());
  }
  const auto ref_counted_memory =
      RefCountedString16::TakeString(std::move(url_list_string));
  data_.emplace(mime_type, ref_counted_memory);

  // Process pending receive requests for this mime type, if there are any.
  auto it = pending_receive_requests_.begin();
  while (it != pending_receive_requests_.end()) {
    if (it->first == mime_type) {
      WriteFileDescriptor(std::move(it->second), ref_counted_memory);
      it = pending_receive_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace exo
