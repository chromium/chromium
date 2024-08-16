// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/clipboard_history/clipboard_history_util.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace chromeos::clipboard_history {

namespace {

// The DIP size of a menu item icon that indicates the clipboard data format.
constexpr int kIconSize = 20;

QueryItemDescriptorsImpl& GetQueryItemDescriptorsImpl() {
  static base::NoDestructor<QueryItemDescriptorsImpl>
      query_item_descriptors_impl;
  return *query_item_descriptors_impl;
}

PasteClipboardItemByIdImpl& GetPasteClipboardItemByIdImpl() {
  static base::NoDestructor<PasteClipboardItemByIdImpl> paste_item_by_id_impl;
  return *paste_item_by_id_impl;
}

}  // namespace

bool IsUrl(const std::u16string& text) {
  return GURL(text).is_valid();
}

void SetQueryItemDescriptorsImpl(QueryItemDescriptorsImpl impl) {
  QueryItemDescriptorsImpl& old_impl = GetQueryItemDescriptorsImpl();
  CHECK(old_impl.is_null() || impl.is_null());
  old_impl = impl;
}

QueryItemDescriptorsImpl::ResultType QueryItemDescriptors() {
  // `SetQueryItemDescriptorsImpl()` may not have been called in unit tests.
  if (auto& query_callback = GetQueryItemDescriptorsImpl()) {
    return query_callback.Run();
  }

  return QueryItemDescriptorsImpl::ResultType();
}

void SetPasteClipboardItemByIdImpl(PasteClipboardItemByIdImpl impl) {
  PasteClipboardItemByIdImpl& old_impl = GetPasteClipboardItemByIdImpl();
  CHECK(old_impl.is_null() || impl.is_null());
  old_impl = impl;
}

void PasteClipboardItemById(
    const base::UnguessableToken& id,
    int event_flags,
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) {
  // `SetPasteClipboardItemByIdImpl()` may not have been called in unit tests.
  if (auto& paste_callback = GetPasteClipboardItemByIdImpl()) {
    paste_callback.Run(id, event_flags, paste_source);
  }
}

ui::ImageModel GetIconForDescriptor(
    const crosapi::mojom::ClipboardHistoryItemDescriptor& descriptor) {
  const gfx::VectorIcon* icon = nullptr;
  switch (descriptor.display_format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      // TODO(http://b/275629173): Consider a new display format for URLs.
      icon = (features::IsClipboardHistoryRefreshEnabled() &&
              IsUrl(descriptor.display_text))
                 ? &vector_icons::kLinkIcon
                 : &kTextIcon;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      icon = &kFiletypeImageIcon;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      icon = &vector_icons::kCodeIcon;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile: {
      // If `display_text` is the name of a single file, use the icon
      // corresponding to the file type, if any; otherwise, use a generic
      // multi-file icon.
      icon = descriptor.file_count == 1
                 ? &chromeos::GetIconForPath(base::FilePath(
                       base::UTF16ToUTF8(descriptor.display_text)))
                 : &vector_icons::kContentCopyIcon;
      break;
    }
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED();
  }

  if (icon) {
    // TODO(b/278109818): Double-check the icon color.
    return ui::ImageModel::FromVectorIcon(*icon,
                                          /*color_id=*/ui::kColorSysSecondary,
                                          kIconSize);
  }

  NOTREACHED();
}

}  // namespace chromeos::clipboard_history
