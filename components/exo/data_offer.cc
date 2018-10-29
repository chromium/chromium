// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/file_helper.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

constexpr char kTextMimeTypeUtf8[] = "text/plain;charset=utf-8";
constexpr char kUtf8String[] = "UTF8_STRING";
constexpr char kUriListSeparator[] = "\r\n";

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

void WriteFileDescriptor(base::ScopedFD fd,
                         scoped_refptr<base::RefCountedMemory> memory) {
  if (!base::WriteFileDescriptor(fd.get(),
                                 reinterpret_cast<const char*>(memory->front()),
                                 memory->size()))
    DLOG(ERROR) << "Failed to write drop data";
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

ui::Clipboard::FormatType GetClipboardFormatType() {
  static const char kFormatString[] = "chromium/x-file-system-files";
  static base::NoDestructor<ui::Clipboard::FormatType> format_type(
      ui::Clipboard::GetFormatType(kFormatString));
  return *format_type;
}

}  // namespace

DataOffer::DataOffer(DataOfferDelegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {}

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

void DataOffer::Accept(const std::string& mime_type) {}

void DataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  const auto data_it = data_.find(mime_type);
  if (data_it == data_.end()) {
    DLOG(ERROR) << "Unexpected mime type is requested";
    return;
  }
  if (data_it->second) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&WriteFileDescriptor, std::move(fd), data_it->second));
  } else {
    // Data bytes for this mime type are being processed currently.
    pending_receive_requests_.push_back(
        std::make_pair(mime_type, std::move(fd)));
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
    const std::string text_mime_type =
        std::string(ui::Clipboard::kMimeTypeText);
    data_.emplace(text_mime_type,
                  RefCountedString16::TakeString(std::move(string_content)));
    delegate_->OnOffer(text_mime_type);
    return;
  }
}

void DataOffer::SetClipboardData(FileHelper* file_helper,
                                 const ui::Clipboard& data) {
  DCHECK_EQ(0u, data_.size());
  if (data.IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                             ui::CLIPBOARD_TYPE_COPY_PASTE)) {
    base::string16 content;
    data.ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &content);
    std::string utf8_content = base::UTF16ToUTF8(content);
    scoped_refptr<base::RefCountedString> utf8_ref =
        base::RefCountedString::TakeString(&utf8_content);
    data_.emplace(std::string(kTextMimeTypeUtf8), utf8_ref);
    data_.emplace(std::string(kUtf8String), utf8_ref);
    delegate_->OnOffer(std::string(kTextMimeTypeUtf8));
    delegate_->OnOffer(std::string(kUtf8String));
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
      base::PostTaskWithTraits(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
          base::BindOnce(&WriteFileDescriptor, std::move(it->second),
                         ref_counted_memory));
      it = pending_receive_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace exo
