// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"

#include <stdint.h>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

// The MIME type for the clipboard format for BrowserActionDragData.
const char kClipboardFormatString[] = "chromium/x-browser-actions";

}

BrowserActionDragData::BrowserActionDragData()
    : profile_(nullptr), index_(static_cast<size_t>(-1)) {}

BrowserActionDragData::BrowserActionDragData(const std::string& id, int index)
    : profile_(nullptr), id_(id), index_(index) {}

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
  return profile_ == profile;
}

void BrowserActionDragData::Write(
    Profile* profile, ui::OSExchangeData* data) const {
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
      ui::ClipboardFormatType::GetType(kClipboardFormatString));

  return *format;
}

void BrowserActionDragData::WriteToPickle(Profile* profile,
                                          base::Pickle* pickle) const {
  pickle->WriteBytes(&profile, sizeof(profile));
  pickle->WriteString(id_);
  pickle->WriteUInt64(index_);
}

bool BrowserActionDragData::ReadFromPickle(base::Pickle* pickle) {
  base::PickleIterator data_iterator(*pickle);

  const char* tmp;
  if (!data_iterator.ReadBytes(&tmp, sizeof(profile_)))
    return false;
  memcpy(&profile_, tmp, sizeof(profile_));

  if (!data_iterator.ReadString(&id_))
    return false;

  uint64_t index;
  if (!data_iterator.ReadUInt64(&index))
    return false;
  index_ = static_cast<size_t>(index);

  return true;
}
