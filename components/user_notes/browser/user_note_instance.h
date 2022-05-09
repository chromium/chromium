// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_

#include "base/barrier_closure.h"
#include "base/memory/safe_ref.h"
#include "components/user_notes/model/user_note.h"
#include "ui/gfx/geometry/rect.h"

namespace user_notes {

// A class that represents the manifestation of a note within a specific web
// page.
class UserNoteInstance {
 public:
  explicit UserNoteInstance(base::SafeRef<UserNote> model);
  virtual ~UserNoteInstance();
  UserNoteInstance(const UserNoteInstance&) = delete;
  UserNoteInstance& operator=(const UserNoteInstance&) = delete;

  UserNote& model() const { return *model_; }
  const gfx::Rect& rect() const { return rect_; }

  // If this note is a text-level note, this method kicks off the asynchronous
  // process to set up the Mojo connection with the renderer side. Otherwise, it
  // invokes the provided callback, if any. Marked virtual for tests.
  virtual void InitializeHighlightIfNeeded(base::OnceClosure callback);

  // blink::mojom::AnnotationAgentHost implementation.
  // TODO(gujen): Make this class inherit from AnnotationAgentHost and fix this
  // method signature (add rect param and add override keyword).
  void DidFinishAttachment();

 private:
  // A ref to the backing model of this note instance. The model is owned by
  // |UserNoteService|. The model is expected to outlive this class.
  base::SafeRef<UserNote> model_;

  // A rect that corresponds to the location in the webpage where the associated
  // highlight is. Can be empty if the note is page-level, if the target text
  // could not be found, or if the highlight hasn't been attached yet.
  gfx::Rect rect_;

  // Callback to invoke after the renderer agent has initialized.
  base::OnceClosure did_finish_attachment_callback_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
