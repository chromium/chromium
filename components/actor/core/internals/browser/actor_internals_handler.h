// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_INTERNALS_BROWSER_ACTOR_INTERNALS_HANDLER_H_
#define COMPONENTS_ACTOR_CORE_INTERNALS_BROWSER_ACTOR_INTERNALS_HANDLER_H_

#include "components/actor/public/mojom/actor_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace actor_internals {

// A cross-platform UI handler for chrome://actor-internals/.
//
// This class must be owned by a controller, which implements or owns the
// Delegate, to guarantee that it outlives this handler.
class ActorInternalsHandler : public mojom::PageHandler {
 public:
  // Implemented by embedders to handle platform-specific logic.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Start trace logging. This will present a file picker to the user.
    virtual void StartLogging() = 0;

    // Stop any active trace logging.
    virtual void StopLogging() = 0;
  };

  ActorInternalsHandler(mojo::PendingRemote<mojom::Page> page,
                        mojo::PendingReceiver<mojom::PageHandler> receiver,
                        Delegate* delegate);

  ActorInternalsHandler(const ActorInternalsHandler&) = delete;
  ActorInternalsHandler& operator=(const ActorInternalsHandler&) = delete;

  ~ActorInternalsHandler() override;

  // Called by embedders to display new logs on the page.
  void OnJournalEntryAdded(mojom::JournalEntryPtr entry);

  // mojom::PageHandler:
  void StartLogging() override;
  void StopLogging() override;

 private:
  mojo::Remote<mojom::Page> remote_;
  mojo::Receiver<mojom::PageHandler> receiver_;
  // The delegate creates `this` and must outlive this handler.
  raw_ptr<Delegate> delegate_;
};

}  // namespace actor_internals

#endif  // COMPONENTS_ACTOR_CORE_INTERNALS_BROWSER_ACTOR_INTERNALS_HANDLER_H_
