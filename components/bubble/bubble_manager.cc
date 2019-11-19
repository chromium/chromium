// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include <utility>
#include <vector>

#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"

BubbleManager::BubbleManager() : manager_state_(SHOW_BUBBLES) {}

BubbleManager::~BubbleManager() {
  FinalizePendingRequests();
}

BubbleReference BubbleManager::ShowBubble(
    std::unique_ptr<BubbleDelegate> bubble) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, ITERATING_BUBBLES);
  DCHECK(bubble);

  std::unique_ptr<BubbleController> controller(
      new BubbleController(this, std::move(bubble)));

  BubbleReference bubble_ref = controller->AsWeakPtr();

  switch (manager_state_) {
    case SHOW_BUBBLES:
      controller->Show();
      controllers_.push_back(std::move(controller));
      for (auto& observer : observers_)
        observer.OnBubbleShown(bubble_ref);
      break;
    case NO_MORE_BUBBLES:
      for (auto& observer : observers_)
        observer.OnBubbleNeverShown(controller->AsWeakPtr());
      break;
    default:
      NOTREACHED();
      break;
  }

  return bubble_ref;
}

bool BubbleManager::CloseBubble(BubbleReference bubble,
                                BubbleCloseReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, ITERATING_BUBBLES);
  return CloseAllMatchingBubbles(bubble.get(), nullptr, reason);
}

void BubbleManager::CloseAllBubbles(BubbleCloseReason reason) {
  // The following close reasons don't make sense for multiple bubbles:
  DCHECK_NE(reason, BUBBLE_CLOSE_ACCEPTED);
  DCHECK_NE(reason, BUBBLE_CLOSE_CANCELED);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, ITERATING_BUBBLES);
  CloseAllMatchingBubbles(nullptr, nullptr, reason);
}

void BubbleManager::UpdateAllBubbleAnchors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(manager_state_, ITERATING_BUBBLES);

  // Guard against bubbles being added or removed while iterating the bubbles.
  ManagerState original_state = manager_state_;
  manager_state_ = ITERATING_BUBBLES;
  for (auto& controller : controllers_)
    controller->UpdateAnchorPosition();
  manager_state_ = original_state;
}

void BubbleManager::AddBubbleManagerObserver(BubbleManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BubbleManager::RemoveBubbleManagerObserver(
    BubbleManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

size_t BubbleManager::GetBubbleCountForTesting() const {
  return controllers_.size();
}

void BubbleManager::FinalizePendingRequests() {
  // Return if already "Finalized".
  if (manager_state_ == NO_MORE_BUBBLES)
    return;

  manager_state_ = NO_MORE_BUBBLES;
  CloseAllBubbles(BUBBLE_CLOSE_FORCED);
}

void BubbleManager::CloseBubblesOwnedBy(const content::RenderFrameHost* frame) {
  CloseAllMatchingBubbles(nullptr, frame, BUBBLE_CLOSE_FRAME_DESTROYED);
}

bool BubbleManager::CloseAllMatchingBubbles(
    BubbleController* bubble,
    const content::RenderFrameHost* owner,
    BubbleCloseReason reason) {
  // Specifying both an owning frame and a particular bubble to close doesn't
  // make sense. If we have a frame, all bubbles owned by that frame need to
  // have the opportunity to close. If we want to close a specific bubble, then
  // it should get the close event regardless of which frame owns it. On the
  // other hand, OR'ing the conditions needs a special case in order to be able
  // to close all bubbles, so we disallow passing both until a need appears.
  DCHECK(!bubble || !owner);

  std::vector<std::unique_ptr<BubbleController>> close_queue;

  // Guard against bubbles being added or removed while iterating the bubbles.
  ManagerState original_state = manager_state_;
  manager_state_ = ITERATING_BUBBLES;
  for (auto i = controllers_.begin(); i != controllers_.end();) {
    if ((!bubble || bubble == (*i).get()) &&
        (!owner || (*i)->OwningFrameIs(owner)) && (*i)->ShouldClose(reason)) {
      close_queue.push_back(std::move(*i));
      i = controllers_.erase(i);
    } else {
      ++i;
    }
  }
  manager_state_ = original_state;

  for (auto& controller : close_queue) {
    controller->DoClose(reason);

    for (auto& observer : observers_)
      observer.OnBubbleClosed(controller->AsWeakPtr(), reason);
  }

  return !close_queue.empty();
}
