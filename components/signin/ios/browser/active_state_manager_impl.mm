// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/active_state_manager_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kActiveStateManagerKeyName[] = "active_state_manager";
}  // namespace

// static
bool ActiveStateManager::ExistsForBrowserState(
    web::BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state->GetUserData(kActiveStateManagerKeyName) != nullptr;
}

// static
ActiveStateManager* ActiveStateManager::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(browser_state);

  ActiveStateManagerImpl* active_state_manager =
      static_cast<ActiveStateManagerImpl*>(
          browser_state->GetUserData(kActiveStateManagerKeyName));
  if (!active_state_manager) {
    active_state_manager = new ActiveStateManagerImpl(browser_state);
    browser_state->SetUserData(kActiveStateManagerKeyName,
                               base::WrapUnique(active_state_manager));
  }
  return active_state_manager;
}

ActiveStateManagerImpl::ActiveStateManagerImpl(web::BrowserState* browser_state)
    : browser_state_(browser_state), active_(false) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(browser_state_);
}

ActiveStateManagerImpl::~ActiveStateManagerImpl() {
  for (auto& observer : observer_list_)
    observer.WillBeDestroyed();
  DCHECK(!IsActive());
}

void ActiveStateManagerImpl::SetActive(bool active) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (active == active_) {
    return;
  }
  active_ = active;

  if (active) {
    for (auto& observer : observer_list_)
      observer.OnActive();
  } else {
    for (auto& observer : observer_list_)
      observer.OnInactive();
  }
}

bool ActiveStateManagerImpl::IsActive() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return active_;
}

void ActiveStateManagerImpl::AddObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  observer_list_.AddObserver(obs);
}

void ActiveStateManagerImpl::RemoveObserver(ActiveStateManager::Observer* obs) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  observer_list_.RemoveObserver(obs);
}
