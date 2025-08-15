// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_chip_controller.h"

#include "base/byte_count.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace memory_saver {
namespace {

// The duration that the chip should be expanded for.
constexpr base::TimeDelta kChipVisibleDuration = base::Seconds(12);

}  // namespace

MemorySaverChipController::MemorySaverChipController(
    page_actions::PageActionController& page_action_controller)
    : page_action_controller_(page_action_controller) {
  CHECK(IsPageActionMigrated(PageActionIconType::kMemorySaver));
}

MemorySaverChipController::~MemorySaverChipController() = default;

void MemorySaverChipController::ShowIcon() {
  page_action_controller_->Show(kActionShowMemorySaverChip);
  page_action_controller_->HideSuggestionChip(kActionShowMemorySaverChip);
  CancelChipTimer();

  RecordMemorySaverChipState(MemorySaverChipState::kCollapsed);
}

void MemorySaverChipController::ShowEducationChip() {
  page_action_controller_->OverrideText(
      kActionShowMemorySaverChip,
      l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_CHIP_LABEL));
  page_action_controller_->Show(kActionShowMemorySaverChip);
  page_action_controller_->ShowSuggestionChip(kActionShowMemorySaverChip,
                                              {.should_announce_chip = true});
  StartChipTimer();

  RecordMemorySaverChipState(MemorySaverChipState::kExpandedEducation);
}

void MemorySaverChipController::ShowMemorySavedChip(
    base::ByteCount bytes_saved) {
  page_action_controller_->Show(kActionShowMemorySaverChip);
  page_action_controller_->ShowSuggestionChip(kActionShowMemorySaverChip);
  std::u16string savings_string = ui::FormatBytes(bytes_saved);
  // TODO(crbug.com/376283619): Cover IDS_MEMORY_SAVER_CHIP_WITH_SAVINGS_ACCNAME
  auto chip_text = l10n_util::GetStringFUTF16(
      IDS_MEMORY_SAVER_CHIP_SAVINGS_LABEL, {savings_string});
  page_action_controller_->OverrideText(kActionShowMemorySaverChip, chip_text);
  StartChipTimer();

  RecordMemorySaverChipState(MemorySaverChipState::kExpandedWithSavings);
}

void MemorySaverChipController::Hide() {
  page_action_controller_->Hide(kActionShowMemorySaverChip);
  page_action_controller_->HideSuggestionChip(kActionShowMemorySaverChip);
  CancelChipTimer();
}

void MemorySaverChipController::StartChipTimer() {
  chip_timer_callback_.Reset(base::BindOnce(
      &MemorySaverChipController::OnChipTimeout, this->GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, chip_timer_callback_.callback(), kChipVisibleDuration);
}

void MemorySaverChipController::CancelChipTimer() {
  chip_timer_callback_.Cancel();
}

void MemorySaverChipController::OnChipTimeout() {
  page_action_controller_->HideSuggestionChip(kActionShowMemorySaverChip);
}

}  // namespace memory_saver
