// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/test_guest_view_manager.h"

#include <memory>
#include <utility>

#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace guest_view {

TestGuestViewManager::TestGuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate)
    : GuestViewManager(context, std::move(delegate)),
      num_embedder_processes_destroyed_(0),
      num_guests_created_(0),
      expected_num_guests_created_(0),
      num_views_garbage_collected_(0),
      waiting_for_guests_created_(false),
      waiting_for_attach_(nullptr) {}

TestGuestViewManager::~TestGuestViewManager() {
}

size_t TestGuestViewManager::GetNumGuestsActive() const {
  return guest_web_contents_by_instance_id_.size();
}

size_t TestGuestViewManager::GetNumRemovedInstanceIDs() const {
  return removed_instance_ids_.size();
}

content::WebContents* TestGuestViewManager::GetLastGuestCreated() {
  content::WebContents* web_contents = nullptr;
  for (int i = current_instance_id_; i >= 0; i--) {
    web_contents = GetGuestByInstanceID(i);
    if (web_contents) {
      break;
    }
  }
  return web_contents;
}

void TestGuestViewManager::WaitForAllGuestsDeleted() {
  // Make sure that every guest that was created has been removed.
  for (auto& watcher : guest_web_contents_watchers_)
    watcher->Wait();
}

void TestGuestViewManager::WaitForLastGuestDeleted() {
  // Wait for the last guest that was created to be deleted.
  guest_web_contents_watchers_.back()->Wait();
}

content::WebContents* TestGuestViewManager::WaitForSingleGuestCreated() {
  if (!GetNumGuestsActive()) {
    // Guests have been created and subsequently destroyed.
    if (num_guests_created() > 0)
      return nullptr;
    WaitForNumGuestsCreated(1u);
  }

  return GetLastGuestCreated();
}

content::WebContents* TestGuestViewManager::WaitForNextGuestCreated() {
  created_message_loop_runner_ = new content::MessageLoopRunner();
  created_message_loop_runner_->Run();
  return GetLastGuestCreated();
}

void TestGuestViewManager::WaitForNumGuestsCreated(size_t count) {
  if (count == num_guests_created_)
    return;

  waiting_for_guests_created_ = true;
  expected_num_guests_created_ = count;

  num_created_message_loop_runner_ = new content::MessageLoopRunner;
  num_created_message_loop_runner_->Run();
}

void TestGuestViewManager::WaitUntilAttached(
    content::WebContents* guest_web_contents) {
  GuestViewBase* guest = GuestViewBase::FromWebContents(guest_web_contents);

  if (guest->attached())
    return;

  waiting_for_attach_ = guest;

  attached_message_loop_runner_ = new content::MessageLoopRunner;
  attached_message_loop_runner_->Run();
}

void TestGuestViewManager::WaitForViewGarbageCollected() {
  gc_message_loop_runner_ = new content::MessageLoopRunner;
  gc_message_loop_runner_->Run();
}

void TestGuestViewManager::WaitForSingleViewGarbageCollected() {
  if (!num_views_garbage_collected())
    WaitForViewGarbageCollected();
}

void TestGuestViewManager::AddGuest(int guest_instance_id,
                                    content::WebContents* guest_web_contents) {
  GuestViewManager::AddGuest(guest_instance_id, guest_web_contents);

  guest_web_contents_watchers_.push_back(
      std::make_unique<content::WebContentsDestroyedWatcher>(
          guest_web_contents));

  if (created_message_loop_runner_)
    created_message_loop_runner_->Quit();

  ++num_guests_created_;
  if (!waiting_for_guests_created_ &&
      num_guests_created_ != expected_num_guests_created_) {
    return;
  }

  if (num_created_message_loop_runner_)
    num_created_message_loop_runner_->Quit();
}

void TestGuestViewManager::AttachGuest(int embedder_process_id,
                                       int element_instance_id,
                                       int guest_instance_id,
                                       const base::Value::Dict& attach_params) {
  GuestViewManager::AttachGuest(embedder_process_id, element_instance_id,
                                guest_instance_id, attach_params);

  if (waiting_for_attach_ &&
      (waiting_for_attach_ ==
       GuestViewBase::From(embedder_process_id, guest_instance_id))) {
    attached_message_loop_runner_->Quit();
    waiting_for_attach_ = nullptr;
  }
}

void TestGuestViewManager::GetGuestWebContentsList(
    std::vector<content::WebContents*>* guest_web_contents_list) {
  for (auto& watcher : guest_web_contents_watchers_)
    guest_web_contents_list->push_back(watcher->web_contents());
}

void TestGuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestViewManager::RemoveGuest(guest_instance_id);
}

void TestGuestViewManager::EmbedderProcessDestroyed(int embedder_process_id) {
  ++num_embedder_processes_destroyed_;
  GuestViewManager::EmbedderProcessDestroyed(embedder_process_id);
}

void TestGuestViewManager::ViewGarbageCollected(int embedder_process_id,
                                                int view_instance_id) {
  GuestViewManager::ViewGarbageCollected(embedder_process_id, view_instance_id);
  ++num_views_garbage_collected_;
  if (gc_message_loop_runner_)
    gc_message_loop_runner_->Quit();
}

// Test factory for creating test instances of GuestViewManager.
TestGuestViewManagerFactory::TestGuestViewManagerFactory()
    : test_guest_view_manager_(nullptr) {}

TestGuestViewManagerFactory::~TestGuestViewManagerFactory() {
}

GuestViewManager* TestGuestViewManagerFactory::CreateGuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate) {
  if (!test_guest_view_manager_) {
    test_guest_view_manager_ =
        new TestGuestViewManager(context, std::move(delegate));
  }
  return test_guest_view_manager_;
}

}  // namespace guest_view
