// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"

#include <stdint.h>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"

namespace {

// The MIME type for the clipboard format for BrowserActionDragData.
const char kClipboardFormatString[] = "chromium/x-browser-actions";

}  // namespace

BrowserActionDragData::BrowserActionDragData()
    : index_(static_cast<size_t>(-1)) {}

BrowserActionDragData::BrowserActionDragData(const std::string& id, int index)
    : id_(id), index_(index) {}

bool BrowserActionDragData::GetDropFormats(
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(GetBrowserActionFormatType());
  return true;
}

bool BrowserActionDragData::AreDropTypesRequired() {
  return true;
}

bool BrowserActionDragData::CanDrop(const ui::OSExchangeData& data,
                                    const Profile* profile) {
  BrowserActionDragData drop_data;
  return drop_data.Read(data) && drop_data.IsFromProfile(profile);
}

bool BrowserActionDragData::IsFromProfile(const Profile* profile) const {
  return profile_unique_token_ == profile->UniqueToken();
}

void BrowserActionDragData::Write(Profile* profile,
                                  ui::OSExchangeData* data) const {
  DCHECK(data);
  base::Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);
  data->SetPickledData(GetBrowserActionFormatType(), data_pickle);
}

bool BrowserActionDragData::Read(const ui::OSExchangeData& data) {
  if (!data.HasCustomFormat(GetBrowserActionFormatType())) {
    return false;
  }

  std::optional<base::Pickle> drag_data_pickle =
      data.GetPickledData(GetBrowserActionFormatType());
  if (!drag_data_pickle.has_value()) {
    return false;
  }

  if (!ReadFromPickle(&drag_data_pickle.value())) {
    return false;
  }

  return true;
}

// static
const ui::ClipboardFormatType&
BrowserActionDragData::GetBrowserActionFormatType() {
  static base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::CustomPlatformType(kClipboardFormatString));

  return *format;
}

void BrowserActionDragData::WriteToPickle(Profile* profile,
                                          base::Pickle* pickle) const {
  pickle->WriteUInt64(profile->UniqueToken().GetHighForSerialization());
  pickle->WriteUInt64(profile->UniqueToken().GetLowForSerialization());
  pickle->WriteString(id_);
  pickle->WriteUInt64(index_);
}

bool BrowserActionDragData::ReadFromPickle(base::Pickle* pickle) {
  base::PickleIterator data_iterator(*pickle);

  uint64_t token_high = 0;
  uint64_t token_low = 0;
  if (!data_iterator.ReadUInt64(&token_high) ||
      !data_iterator.ReadUInt64(&token_low)) {
    return false;
  }
  auto token = base::UnguessableToken::Deserialize(token_high, token_low);
  if (!token) {
    return false;
  }
  profile_unique_token_ = *token;

  if (!data_iterator.ReadString(&id_)) {
    return false;
  }

  uint64_t index;
  if (!data_iterator.ReadUInt64(&index)) {
    return false;
  }
  index_ = static_cast<size_t>(index);

  return true;
}
