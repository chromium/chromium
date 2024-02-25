// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_UNDO_MANAGER_H_
#define COMPONENTS_UNDO_UNDO_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

class UndoManagerObserver;
class UndoOperation;

// UndoGroup ------------------------------------------------------------------

// UndoGroup represents a user action and stores all the operations that
// make that action.  Typically there is only one operation per UndoGroup.
class UndoGroup {
 public:
  UndoGroup();

  UndoGroup(const UndoGroup&) = delete;
  UndoGroup& operator=(const UndoGroup&) = delete;

  ~UndoGroup();

  void AddOperation(std::unique_ptr<UndoOperation> operation);
  const std::vector<std::unique_ptr<UndoOperation>>& undo_operations() {
    return operations_;
  }
  void Undo();

  // The resource string id describing the undo and redo action.
  int get_undo_label_id() const { return undo_label_id_; }
  void set_undo_label_id(int label_id) { undo_label_id_ = label_id; }

  int get_redo_label_id() const { return redo_label_id_; }
  void set_redo_label_id(int label_id) { redo_label_id_ = label_id; }

 private:
  std::vector<std::unique_ptr<UndoOperation>> operations_;

  // The resource string id describing the undo and redo action.
  int undo_label_id_;
  int redo_label_id_;
};

// UndoManager ----------------------------------------------------------------

// Maintains user actions as a group of operations that store enough info to
// undo and redo those operations.
class UndoManager {
 public:
  UndoManager();

  UndoManager(const UndoManager&) = delete;
  UndoManager& operator=(const UndoManager&) = delete;

  ~UndoManager();

  // Perform an undo or redo operation.
  void Undo();
  void Redo();

  size_t undo_count() const { return undo_actions_.size(); }
  size_t redo_count() const { return redo_actions_.size(); }

  std::u16string GetUndoLabel() const;
  std::u16string GetRedoLabel() const;

  void AddUndoOperation(std::unique_ptr<UndoOperation> operation);

  // Group multiple operations into one undoable action.
  void StartGroupingActions();
  void EndGroupingActions();

  // Suspend undo tracking while processing non-user initiated changes such as
  // profile synchonization.
  void SuspendUndoTracking();
  void ResumeUndoTracking();
  bool IsUndoTrakingSuspended() const;

  // Remove all undo and redo operations. Note that grouping of actions and
  // suspension of undo tracking states are left unchanged.
  void RemoveAllOperations();

  // Observers are notified when the internal state of this class changes.
  void AddObserver(UndoManagerObserver* observer);
  void RemoveObserver(UndoManagerObserver* observer);

 private:
  friend class UndoManagerTestApi;

  void Undo(bool* performing_indicator,
            std::vector<std::unique_ptr<UndoGroup>>* active_undo_group);
  bool is_user_action() const { return !performing_undo_ && !performing_redo_; }

  // Notifies the observers that the undo manager's state has changed.
  void NotifyOnUndoManagerStateChange();

  // Handle the addition of |new_undo_group| to the active undo group container.
  void AddUndoGroup(std::unique_ptr<UndoGroup> new_undo_group);

  // Returns the undo or redo UndoGroup container that should store the next
  // change taking into account if an undo or redo is being executed.
  std::vector<std::unique_ptr<UndoGroup>>* GetActiveUndoGroup();

  // Containers of user actions ready for an undo or redo treated as a stack.
  std::vector<std::unique_ptr<UndoGroup>> undo_actions_;
  std::vector<std::unique_ptr<UndoGroup>> redo_actions_;

  // The observers to notify when internal state changes.
  base::ObserverList<UndoManagerObserver>::Unchecked observers_;

  // Supports grouping operations into a single undo action.
  int group_actions_count_;

  // The container that is used when actions are grouped.
  std::unique_ptr<UndoGroup> pending_grouped_action_;

  // The action that is in the process of being undone.
  raw_ptr<UndoGroup> undo_in_progress_action_;

  // Supports the suspension of undo tracking.
  int undo_suspended_count_;

  // Set when executing Undo or Redo so that incoming changes are correctly
  // processed.
  bool performing_undo_;
  bool performing_redo_;
};

#endif  // COMPONENTS_UNDO_UNDO_MANAGER_H_
