// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/available_offline_content_helper.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"

namespace {

using chrome::mojom::AvailableOfflineContentPtr;
using chrome::mojom::AvailableContentType;

// Converts a string to base-64 data. This is done for security purposes, to
// avoid potential XSS. Note that when this value is decoded in javascript, we
// want to use the atob() function, but that function only handles latin-1
// characters. Additionally, javascript needs UTF16 strings. So we instead
// encode to UTF16, and then store that data as base64.
std::string ConvertToUTF16Base64(const std::string& text) {
  base::string16 text_utf16 = base::UTF8ToUTF16(text);
  std::string utf16_bytes;
  for (base::char16 c : text_utf16) {
    utf16_bytes.push_back(static_cast<char>(c >> 8));
    utf16_bytes.push_back(static_cast<char>(c & 0xff));
  }
  std::string encoded;
  base::Base64Encode(utf16_bytes, &encoded);
  return encoded;
}

base::Value AvailableContentToValue(const AvailableOfflineContentPtr& content) {
  // All pieces of text content downloaded from the web will be base64 encoded
  // to lessen security risks when this dictionary is passed as a string to
  // |ExecuteJavaScript|.
  std::string base64_encoded;
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("ID", base::Value(content->id));
  value.SetKey("name_space", base::Value(content->name_space));
  value.SetKey("title_base64",
               base::Value(ConvertToUTF16Base64(content->title)));
  value.SetKey("snippet_base64",
               base::Value(ConvertToUTF16Base64(content->snippet)));
  value.SetKey("date_modified", base::Value(content->date_modified));
  value.SetKey("attribution_base64",
               base::Value(ConvertToUTF16Base64(content->attribution)));
  value.SetKey("thumbnail_data_uri",
               base::Value(content->thumbnail_data_uri.spec()));
  value.SetKey("favicon_data_uri",
               base::Value(content->favicon_data_uri.spec()));
  value.SetKey("content_type",
               base::Value(static_cast<int>(content->content_type)));
  return value;
}

base::Value AvailableContentListToValue(
    const std::vector<AvailableOfflineContentPtr>& content_list) {
  base::Value value(base::Value::Type::LIST);
  for (const auto& content : content_list) {
    value.Append(AvailableContentToValue(content));
  }
  return value;
}

void RecordSuggestionPresented(
    const std::vector<AvailableOfflineContentPtr>& suggestions) {
  for (const AvailableOfflineContentPtr& item : suggestions) {
    UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts.SuggestionPresented",
                              item->content_type);
  }
}

AvailableOfflineContentHelper::Binder& GetBinderOverride() {
  static base::NoDestructor<AvailableOfflineContentHelper::Binder> binder;
  return *binder;
}

}  // namespace

AvailableOfflineContentHelper::AvailableOfflineContentHelper() = default;
AvailableOfflineContentHelper::~AvailableOfflineContentHelper() = default;

void AvailableOfflineContentHelper::Reset() {
  provider_.reset();
}

void AvailableOfflineContentHelper::FetchAvailableContent(
    AvailableContentCallback callback) {
  if (!BindProvider()) {
    std::move(callback).Run(true, {});
    return;
  }
  provider_->List(
      base::BindOnce(&AvailableOfflineContentHelper::AvailableContentReceived,
                     base::Unretained(this), std::move(callback)));
}

bool AvailableOfflineContentHelper::BindProvider() {
  if (provider_)
    return true;

  auto receiver = provider_.BindNewPipeAndPassReceiver();
  const auto& binder_override = GetBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return true;
  }

  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      std::move(receiver));
  return true;
}

// static
void AvailableOfflineContentHelper::OverrideBinderForTesting(Binder binder) {
  GetBinderOverride() = std::move(binder);
}

void AvailableOfflineContentHelper::LaunchItem(const std::string& id,
                                               const std::string& name_space) {
  if (!BindProvider())
    return;

  for (const AvailableOfflineContentPtr& item : fetched_content_) {
    if (item->id == id && item->name_space == name_space) {
      UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts.SuggestionClicked",
                                item->content_type);
      RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED);
      provider_->LaunchItem(id, name_space);
      return;
    }
  }
  NOTREACHED();
}

void AvailableOfflineContentHelper::LaunchDownloadsPage() {
  if (!BindProvider())
    return;
  RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED);
  provider_->LaunchDownloadsPage(has_prefetched_content_);
}

void AvailableOfflineContentHelper::ListVisibilityChanged(bool is_visible) {
  if (!BindProvider())
    return;
  provider_->ListVisibilityChanged(is_visible);
}

void AvailableOfflineContentHelper::AvailableContentReceived(
    AvailableContentCallback callback,
    bool list_visible_by_prefs,
    std::vector<AvailableOfflineContentPtr> content) {
  has_prefetched_content_ = false;
  fetched_content_ = std::move(content);
  std::string json;

  if (!fetched_content_.empty()) {
    // As prefetched content has the highest priority if at least one piece is
    // available it will be the at the first position on the list.
    has_prefetched_content_ = fetched_content_.front()->content_type ==
                              AvailableContentType::kPrefetchedPage;

    RecordSuggestionPresented(fetched_content_);
    if (list_visible_by_prefs)
      RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN);
    else
      RecordEvent(
          error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED);
    base::JSONWriter::Write(AvailableContentListToValue(fetched_content_),
                            &json);
  }
  std::move(callback).Run(list_visible_by_prefs, json);
  // We don't need to retain the visuals here, so free up some memory.
  for (const AvailableOfflineContentPtr& item : fetched_content_) {
    item->thumbnail_data_uri = GURL();
    item->favicon_data_uri = GURL();
  }
}

