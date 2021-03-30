// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/font_access/font_access_chooser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/font_access_chooser.h"
#include "content/public/browser/font_access_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

content::FontAccessContext* GetChooserContext(content::RenderFrameHost* frame) {
  return frame->GetStoragePartition()->GetFontAccessContext();
}

}  // namespace

FontAccessChooserController::FontAccessChooserController(
    content::RenderFrameHost* frame,
    const std::vector<std::string>& selection,
    content::FontAccessChooser::Callback callback)
    : ChooserController(frame,
                        IDS_FONT_ACCESS_CHOOSER_PROMPT_ORIGIN,
                        // Extensions are not supported. This is stub text.
                        IDS_FONT_ACCESS_CHOOSER_PROMPT_ORIGIN),
      callback_(std::move(callback)) {
  DCHECK(frame);

  content::FontAccessContext* chooser_context = GetChooserContext(frame);
  if (chooser_context == nullptr) {
    std::move(callback_).Run(
        blink::mojom::FontEnumerationStatus::kUnexpectedError, {});
    return;
  }

  selection_ = base::flat_set<std::string>(selection);

  chooser_context->FindAllFonts(
      base::BindOnce(&FontAccessChooserController::DidFindAllFonts,
                     weak_factory_.GetWeakPtr()));
}

FontAccessChooserController::~FontAccessChooserController() {
  if (callback_) {
    std::move(callback_).Run(
        blink::mojom::FontEnumerationStatus::kUnexpectedError, {});
  }
}

std::u16string FontAccessChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_FONT_ACCESS_CHOOSER_NO_FONTS_FOUND_PROMPT);
}

std::u16string FontAccessChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_FONT_ACCESS_CHOOSER_IMPORT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
FontAccessChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_FONT_ACCESS_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_FONT_ACCESS_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t FontAccessChooserController::NumOptions() const {
  return items_.size();
}

std::u16string FontAccessChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, items_.size());
  DCHECK(base::Contains(font_metadata_map_, items_[index]));

  return base::UTF8ToUTF16(items_[index]);
}

std::u16string FontAccessChooserController::GetSelectAllCheckboxLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_FONT_ACCESS_CHOOSER_SELECT_ALL_CHECKBOX_TEXT);
}

bool FontAccessChooserController::ShouldShowHelpButton() const {
  return false;
}

bool FontAccessChooserController::ShouldShowReScanButton() const {
  return false;
}

bool FontAccessChooserController::BothButtonsAlwaysEnabled() const {
  // Import button is disabled if there isn't at least one selection.
  return false;
}

bool FontAccessChooserController::AllowMultipleSelection() const {
  return true;
}

bool FontAccessChooserController::ShouldShowSelectAllCheckbox() const {
  return true;
}

bool FontAccessChooserController::TableViewAlwaysDisabled() const {
  return false;
}

void FontAccessChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_GT(indices.size(), 0u);

  std::vector<blink::mojom::FontMetadataPtr> fonts;

  for (auto& index : indices) {
    DCHECK_LT(index, items_.size());
    auto found = font_metadata_map_.find(items_[index]);
    if (found == font_metadata_map_.end()) {
      continue;
    }
    fonts.push_back(found->second.Clone());
  }

  std::move(callback_).Run(blink::mojom::FontEnumerationStatus::kOk,
                           std::move(fonts));
}

void FontAccessChooserController::Cancel() {
  std::move(callback_).Run(blink::mojom::FontEnumerationStatus::kCanceled, {});
}

void FontAccessChooserController::Close() {
  std::move(callback_).Run(blink::mojom::FontEnumerationStatus::kCanceled, {});
}

void FontAccessChooserController::OpenHelpCenterUrl() const {
  NOTIMPLEMENTED();
}

void FontAccessChooserController::DidFindAllFonts(
    blink::mojom::FontEnumerationStatus status,
    std::vector<blink::mojom::FontMetadata> fonts) {
  for (auto& font : fonts) {
    auto found = font_metadata_map_.find(font.postscript_name);
    // If the font is already in the map, skip it.
    if (found != font_metadata_map_.end()) {
      continue;
    }
    // If a selection list is provided and the font isn't in the list, skip it.
    if (!selection_.empty() && !selection_.contains(font.postscript_name)) {
      continue;
    }
    items_.push_back(font.postscript_name);
    font_metadata_map_[font.postscript_name] = std::move(font);
  }
  if (view())
    view()->OnOptionsInitialized();

  if (ready_callback_for_testing_)
    std::move(ready_callback_for_testing_).Run();
}
