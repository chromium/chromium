// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_UNDO_OPERATION_H_
#define COMPONENTS_UNDO_UNDO_OPERATION_H_

// Base class for all undo operations.
class UndoOperation {
 public:
  virtual ~UndoOperation() = default;

  virtual void Undo() = 0;

  // Returns the resource string id describing the undo/redo of this operation
  // for use as labels in the UI.
  // Note: The labels describe the original user action, this may result in
  // the meaning of the redo label being reversed. For example, an
  // UndoOperation representing a deletion would have been created in order to
  // redo an addition by the user. In this case, the redo label string for the
  // UndoOperation of delete would be "Redo add".
  virtual int GetUndoLabelId() const = 0;
  virtual int GetRedoLabelId() const = 0;
};

#endif  // COMPONENTS_UNDO_UNDO_OPERATION_H_
