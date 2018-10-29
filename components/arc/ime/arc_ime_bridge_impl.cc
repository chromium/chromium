// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_bridge_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {
namespace {

std::vector<mojom::CompositionSegmentPtr> ConvertSegments(
    const ui::CompositionText& composition) {
  std::vector<mojom::CompositionSegmentPtr> segments;
  for (const ui::ImeTextSpan& ime_text_span : composition.ime_text_spans) {
    mojom::CompositionSegmentPtr segment = mojom::CompositionSegment::New();
    segment->start_offset = ime_text_span.start_offset;
    segment->end_offset = ime_text_span.end_offset;
    segment->emphasized =
        (ime_text_span.thickness == ui::ImeTextSpan::Thickness::kThick ||
         (composition.selection.start() == ime_text_span.start_offset &&
          composition.selection.end() == ime_text_span.end_offset));
    segments.push_back(std::move(segment));
  }
  return segments;
}

}  // namespace

ArcImeBridgeImpl::ArcImeBridgeImpl(Delegate* delegate,
                                   ArcBridgeService* bridge_service)
    : delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->ime()->SetHost(this);
}

ArcImeBridgeImpl::~ArcImeBridgeImpl() {
  bridge_service_->ime()->SetHost(nullptr);
}

void ArcImeBridgeImpl::SendSetCompositionText(
    const ui::CompositionText& composition) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), SetCompositionText);
  if (!ime_instance)
    return;

  ime_instance->SetCompositionText(base::UTF16ToUTF8(composition.text),
                                   ConvertSegments(composition));
}

void ArcImeBridgeImpl::SendConfirmCompositionText() {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(),
                                                   ConfirmCompositionText);
  if (!ime_instance)
    return;

  ime_instance->ConfirmCompositionText();
}

void ArcImeBridgeImpl::SendInsertText(const base::string16& text) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), InsertText);
  if (!ime_instance)
    return;

  ime_instance->InsertText(base::UTF16ToUTF8(text));
}

void ArcImeBridgeImpl::SendExtendSelectionAndDelete(
    size_t before, size_t after) {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(),
                                                   ExtendSelectionAndDelete);
  if (!ime_instance)
    return;

  ime_instance->ExtendSelectionAndDelete(before, after);
}

void ArcImeBridgeImpl::SendOnKeyboardAppearanceChanging(
    const gfx::Rect& new_bounds,
    bool is_available) {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->ime(), OnKeyboardAppearanceChanging);
  if (!ime_instance)
    return;

  ime_instance->OnKeyboardAppearanceChanging(new_bounds, is_available);
}

void ArcImeBridgeImpl::OnTextInputTypeChanged(
    ui::TextInputType type,
    bool is_personalized_learning_allowed) {
  delegate_->OnTextInputTypeChanged(type, is_personalized_learning_allowed);
}

void ArcImeBridgeImpl::OnCursorRectChanged(const gfx::Rect& rect,
                                           bool is_screen_coordinates) {
  delegate_->OnCursorRectChanged(rect, is_screen_coordinates);
}

void ArcImeBridgeImpl::OnCancelComposition() {
  delegate_->OnCancelComposition();
}

void ArcImeBridgeImpl::ShowVirtualKeyboardIfEnabled() {
  delegate_->ShowVirtualKeyboardIfEnabled();
}

void ArcImeBridgeImpl::OnCursorRectChangedWithSurroundingText(
    const gfx::Rect& rect,
    const gfx::Range& text_range,
    const std::string& text_in_range,
    const gfx::Range& selection_range,
    bool is_screen_coordinates) {
  delegate_->OnCursorRectChangedWithSurroundingText(
      rect, text_range, base::UTF8ToUTF16(text_in_range), selection_range,
      is_screen_coordinates);
}

void ArcImeBridgeImpl::RequestHideIme() {
  delegate_->RequestHideIme();
}

}  // namespace arc
