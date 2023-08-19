// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_

#include <memory>

#include "base/barrier_closure.h"
#include "base/memory/safe_ref.h"
#include "base/types/pass_key.h"
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
  using PassKey = base::PassKey<UserNoteInstance>;

  // The callback type invoked when attachment in the renderer is completed.
  using AttachmentFinishedCallback = base::OnceClosure;

  // Creates a UserNoteInstance. This instance will attach to a highlight in
  // the renderer (if the note is a non-page note) when
  // InitializeHighlightIfNeeded is called.
  static std::unique_ptr<UserNoteInstance> Create(
      base::SafeRef<UserNote> model,
      UserNoteManager* parent_manager);

  // Use the Create static methods above. Public for use with std::make_unique.
  UserNoteInstance(base::SafeRef<UserNote> model,
                   UserNoteManager* parent_manager,
                   PassKey pass_key);

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

  // Binds this instance to an existing highlight in the renderer. Must be
  // called before the instance is added to the manager.
  void BindToHighlight(
      mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
      mojo::PendingRemote<blink::mojom::AnnotationAgent> agent_remote,
      AttachmentFinishedCallback callback);

  // If this note is a text-level note and hasn't yet been bound to a highlight
  // (via BindToHighlight), this method kicks off the asynchronous process to
  // set up the Mojo connection with the corresponding agent in the renderer
  // process. Otherwise, it invokes the provided callback. Marked virtual for
  // tests to override.
  virtual void InitializeHighlightIfNeeded(AttachmentFinishedCallback callback);

  void OnNoteSelected();

  // blink::mojom::AnnotationAgentHost implementation.
  void DidFinishAttachment(const gfx::Rect& rect) override;

  // TODO(corising) and TODO(bokan): add the following method to
  // AnnotationAgentHost interface once the caller is implemented in Blink.
  void OnWebHighlightFocused();

  // TODO(gujen) and TODO(bokan): add the following method to the
  // AnnotationAgentHost interface so it's called when a note becomes detached.
  // Mark this one as override.
  void OnNoteDetached();

 protected:
  // For mock descendants in tests since they can't instantiate the PassKey.
  UserNoteInstance(base::SafeRef<UserNote> model,
                   UserNoteManager* parent_manager);

 private:
  friend class UserNoteInstanceTest;

  // Performs the actual Mojo initialization. Marked virtual for tests to
  // override.
  virtual void InitializeHighlightInternal();

  // A ref to the backing model of this note instance. The model is owned by
  // |UserNoteService|. The model is expected to outlive this class.
  base::SafeRef<UserNote, base::SafeRefDanglingUntriaged> model_;

  // The owning note manager.
  raw_ptr<UserNoteManager> parent_manager_;

  // A rect that corresponds to the location in the webpage where the associated
  // highlight is. Can be empty if the note is page-level, if the target text
  // could not be found, or if the highlight hasn't been attached yet.
  gfx::Rect rect_;

  // Callback to invoke after the renderer agent has initialized.
  AttachmentFinishedCallback did_finish_attachment_callback_;

  // Stores whether this note instance has had its highlight initialized
  // ("attached") in the renderer process. This will be true, even if
  // attachment failed.
  bool finished_attachment_ = false;

  // Receiver and agent for communication with the renderer process.
  mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
