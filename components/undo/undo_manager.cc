// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/undo_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/undo_manager_observer.h"
#include "components/undo/undo_operation.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Maximum number of changes that can be undone.
const size_t kMaxUndoGroups = 100;

}  // namespace

// UndoGroup ------------------------------------------------------------------

UndoGroup::UndoGroup()
    : undo_label_id_(IDS_BOOKMARK_BAR_UNDO),
      redo_label_id_(IDS_BOOKMARK_BAR_REDO) {
}

UndoGroup::~UndoGroup() {
}

void UndoGroup::AddOperation(std::unique_ptr<UndoOperation> operation) {
  if (operations_.empty()) {
    set_undo_label_id(operation->GetUndoLabelId());
    set_redo_label_id(operation->GetRedoLabelId());
  }
  operations_.push_back(std::move(operation));
}

void UndoGroup::Undo() {
  for (const std::unique_ptr<UndoOperation>& operation :
       base::Reversed(operations_))
    operation->Undo();
}

// UndoManager ----------------------------------------------------------------

UndoManager::UndoManager()
    : group_actions_count_(0),
      undo_in_progress_action_(nullptr),
      undo_suspended_count_(0),
      performing_undo_(false),
      performing_redo_(false) {}

UndoManager::~UndoManager() {
  DCHECK_EQ(0, group_actions_count_);
  DCHECK_EQ(0, undo_suspended_count_);
  DCHECK(!performing_undo_);
  DCHECK(!performing_redo_);
}

void UndoManager::Undo() {
  Undo(&performing_undo_, &undo_actions_);
}

void UndoManager::Redo() {
  Undo(&performing_redo_, &redo_actions_);
}

std::u16string UndoManager::GetUndoLabel() const {
  return l10n_util::GetStringUTF16(
      undo_actions_.empty() ? IDS_BOOKMARK_BAR_UNDO
                            : undo_actions_.back()->get_undo_label_id());
}

std::u16string UndoManager::GetRedoLabel() const {
  return l10n_util::GetStringUTF16(
      redo_actions_.empty() ? IDS_BOOKMARK_BAR_REDO
                            : redo_actions_.back()->get_redo_label_id());
}

void UndoManager::AddUndoOperation(std::unique_ptr<UndoOperation> operation) {
  if (IsUndoTrakingSuspended()) {
    RemoveAllOperations();
    operation.reset();
    return;
  }

  if (group_actions_count_) {
    pending_grouped_action_->AddOperation(std::move(operation));
  } else {
    auto new_action = std::make_unique<UndoGroup>();
    new_action->AddOperation(std::move(operation));
    AddUndoGroup(std::move(new_action));
  }
}

void UndoManager::StartGroupingActions() {
  if (!group_actions_count_)
    pending_grouped_action_ = std::make_unique<UndoGroup>();
  ++group_actions_count_;
}

void UndoManager::EndGroupingActions() {
  --group_actions_count_;
  if (group_actions_count_ > 0)
    return;

  // Check that StartGroupingActions and EndGroupingActions are paired.
  DCHECK_GE(group_actions_count_, 0);

  bool is_user_action = !performing_undo_ && !performing_redo_;
  if (!pending_grouped_action_->undo_operations().empty()) {
    AddUndoGroup(std::move(pending_grouped_action_));
  } else {
    // No changes were executed since we started grouping actions, so the
    // pending UndoGroup should be discarded.
    pending_grouped_action_.reset();

    // This situation is only expected when it is a user initiated action.
    // Undo/Redo should have at least one operation performed.
    DCHECK(is_user_action);
  }
}

void UndoManager::SuspendUndoTracking() {
  ++undo_suspended_count_;
}

void UndoManager::ResumeUndoTracking() {
  DCHECK_GT(undo_suspended_count_, 0);
  --undo_suspended_count_;
}

bool UndoManager::IsUndoTrakingSuspended() const {
  return undo_suspended_count_ > 0;
}

void UndoManager::RemoveAllOperations() {
  DCHECK(!group_actions_count_);
  undo_actions_.clear();
  redo_actions_.clear();

  NotifyOnUndoManagerStateChange();
}

void UndoManager::AddObserver(UndoManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void UndoManager::RemoveObserver(UndoManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UndoManager::Undo(
    bool* performing_indicator,
    std::vector<std::unique_ptr<UndoGroup>>* active_undo_group) {
  // Check that action grouping has been correctly ended.
  DCHECK(!group_actions_count_);

  if (active_undo_group->empty())
    return;

  base::AutoReset<bool> incoming_changes(performing_indicator, true);
  std::unique_ptr<UndoGroup> action = std::move(active_undo_group->back());
  base::AutoReset<raw_ptr<UndoGroup>> action_context(&undo_in_progress_action_,
                                                     action.get());
  active_undo_group->erase(active_undo_group->begin() +
                           active_undo_group->size() - 1);

  StartGroupingActions();
  action->Undo();
  EndGroupingActions();

  NotifyOnUndoManagerStateChange();
}

void UndoManager::NotifyOnUndoManagerStateChange() {
  for (auto& observer : observers_)
    observer.OnUndoManagerStateChange();
}

void UndoManager::AddUndoGroup(std::unique_ptr<UndoGroup> new_undo_group) {
  GetActiveUndoGroup()->push_back(std::move(new_undo_group));

  // User actions invalidate any available redo actions.
  if (is_user_action())
    redo_actions_.clear();

  // Limit the number of undo levels so the undo stack does not grow unbounded.
  if (GetActiveUndoGroup()->size() > kMaxUndoGroups)
    GetActiveUndoGroup()->erase(GetActiveUndoGroup()->begin());

  NotifyOnUndoManagerStateChange();
}

std::vector<std::unique_ptr<UndoGroup>>* UndoManager::GetActiveUndoGroup() {
  return performing_undo_ ? &redo_actions_ : &undo_actions_;
}
