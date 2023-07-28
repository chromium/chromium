// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_UTIL_H_
#define CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ui {
class ImageModel;
}  // namespace ui

namespace chromeos::clipboard_history {

// Returns whether the specified `text` represents a valid URL.
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
bool IsUrl(const std::u16string& text);

// Sets the function implementation that queries for the clipboard history item
// descriptors. CrOS Ash and CrOS Lacros have different implementations.
using QueryItemDescriptorsImpl = base::RepeatingCallback<
    std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>()>;
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
void SetQueryItemDescriptorsImpl(QueryItemDescriptorsImpl impl);

// Queries for the clipboard history item descriptors.
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
QueryItemDescriptorsImpl::ResultType QueryItemDescriptors();

// Sets the function implementation that pastes the clipboard item specified
// by id. CrOS Ash and CrOS Lacros have different implementations.
using PasteClipboardItemByIdImpl = base::RepeatingCallback<void(
    const base::UnguessableToken&,
    int,
    crosapi::mojom::ClipboardHistoryControllerShowSource)>;
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
void SetPasteClipboardItemByIdImpl(PasteClipboardItemByIdImpl impl);

// Pastes the clipboard item specified by `id`.
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
void PasteClipboardItemById(
    const base::UnguessableToken& id,
    int event_flags,
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source);

// Returns the icon that represents the `descriptor`.
COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
ui::ImageModel GetIconForDescriptor(
    const crosapi::mojom::ClipboardHistoryItemDescriptor& descriptor);

}  // namespace chromeos::clipboard_history

#endif  // CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_UTIL_H_
