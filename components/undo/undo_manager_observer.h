// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_UNDO_MANAGER_OBSERVER_H_
#define COMPONENTS_UNDO_UNDO_MANAGER_OBSERVER_H_

// Observer for the UndoManager.
class UndoManagerObserver {
 public:
  // Invoked when the internal state of the UndoManager has changed.
  virtual void OnUndoManagerStateChange() = 0;

 protected:
  virtual ~UndoManagerObserver() = default;
};

#endif  // COMPONENTS_UNDO_UNDO_MANAGER_OBSERVER_H_
