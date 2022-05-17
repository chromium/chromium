// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_

#include "base/barrier_closure.h"
#include "base/memory/safe_ref.h"
#include "components/user_notes/model/user_note.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace user_notes {

class UserNoteManager;

// A class that represents the manifestation of a note within a specific web
// page.
class UserNoteInstance : public blink::mojom::AnnotationAgentHost {
 public:
  // The main constructor.
  UserNoteInstance(base::SafeRef<UserNote> model,
                   UserNoteManager* parent_manager);

  // A constructor for when the bounding rect of the highlight is known in
  // advance, for example during the note creation process.
  UserNoteInstance(base::SafeRef<UserNote> model,
                   UserNoteManager* parent_manager,
                   gfx::Rect rect);

  ~UserNoteInstance() override;
  UserNoteInstance(const UserNoteInstance&) = delete;
  UserNoteInstance& operator=(const UserNoteInstance&) = delete;

  UserNote& model() const { return *model_; }
  const gfx::Rect& rect() const { return rect_; }

  // Returns true if the note is detached, false otherwise. A note is considered
  // detached if all of the following are true:
  // 1) It has a target text (i.e. it is not page-level);
  // 2) Its highlight has been initialized already;
  // 3) Its bounding rect is empty.
  bool IsDetached() const;

  // If this note is a text-level note, this method kicks off the asynchronous
  // process to set up the Mojo connection with the corresponding agent in the
  // renderer process. Otherwise, it invokes the provided callback.
  // Marked virtual for tests to override.
  virtual void InitializeHighlightIfNeeded(base::OnceClosure callback);

  // blink::mojom::AnnotationAgentHost implementation.
  void DidFinishAttachment(const gfx::Rect& rect) override;

  // TODO(gujen) and TODO(bokan): add the following method to the
  // AnnotationAgentHost interface so it's called when a note becomes detached.
  // Mark this one as override.
  void OnNoteDetached();

 private:
  friend class UserNoteInstanceTest;

  // Performs the actual Mojo initialization. Marked virtual for tests to
  // override.
  virtual void InitializeHighlightInternal();

  // A ref to the backing model of this note instance. The model is owned by
  // |UserNoteService|. The model is expected to outlive this class.
  base::SafeRef<UserNote> model_;

  // The owning note manager.
  raw_ptr<UserNoteManager> parent_manager_;

  // A rect that corresponds to the location in the webpage where the associated
  // highlight is. Can be empty if the note is page-level, if the target text
  // could not be found, or if the highlight hasn't been attached yet.
  gfx::Rect rect_;

  // Callback to invoke after the renderer agent has initialized.
  base::OnceClosure did_finish_attachment_callback_;

  // Stores whether this note instance has had its highlight initialized in the
  // renderer process.
  bool is_initialized_ = false;

  // Receiver and agent for communication with the renderer process.
  mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
