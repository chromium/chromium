// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/clipboard_history/clipboard_history_util.h"

#include "base/no_destructor.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

namespace chromeos::clipboard_history {

namespace {
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

void SetQueryItemDescriptorsImpl(QueryItemDescriptorsImpl impl) {
  QueryItemDescriptorsImpl& old_impl = GetQueryItemDescriptorsImpl();
  CHECK(old_impl.is_null() || impl.is_null());
  old_impl = impl;
}

QueryItemDescriptorsImpl::ResultType QueryItemDescriptors() {
  return GetQueryItemDescriptorsImpl().Run();
}

void SetPasteClipboardItemByIdImpl(PasteClipboardItemByIdImpl impl) {
  PasteClipboardItemByIdImpl& old_impl = GetPasteClipboardItemByIdImpl();
  CHECK(old_impl.is_null() || impl.is_null());
  old_impl = impl;
}

void PasteClipboardItemById(
    const std::string& id,
    int event_flags,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  GetPasteClipboardItemByIdImpl().Run(id, event_flags, show_source);
}

ui::ImageModel GetIconForDisplayFormat(
    crosapi::mojom::ClipboardHistoryDisplayFormat display_format) {
  // TODO(b/278915828): Add menu item icons for other display formats.
  if (display_format == crosapi::mojom::ClipboardHistoryDisplayFormat::kText) {
    // TODO(b/278109818): Double-check the icon color.
    return ui::ImageModel::FromVectorIcon(kTextIcon, ui::kColorSysSecondary);
  }

  return ui::ImageModel();
}

}  // namespace chromeos::clipboard_history
