// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/test_guest_view_manager.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace {
// Returns the current RFH owned by the FrameTreeNode, denoted by
// |frame_tree_node_id|.
content::RenderFrameHost* GetCurrentGuestMainRenderFrameHost(
    int frame_tree_node_id) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  DCHECK(web_contents);
  return web_contents->UnsafeFindFrameByFrameTreeNodeId(frame_tree_node_id);
}
}  // namespace

namespace guest_view {

TestGuestViewManager::TestGuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate)
    : GuestViewManager(context, std::move(delegate)) {}

TestGuestViewManager::~TestGuestViewManager() = default;

size_t TestGuestViewManager::GetCurrentGuestCount() const {
  return guests_by_instance_id_.size();
}

size_t TestGuestViewManager::GetNumRemovedInstanceIDs() const {
  return removed_instance_ids_.size();
}

content::RenderFrameHost*
TestGuestViewManager::GetLastGuestRenderFrameHostCreated() {
  for (auto it = guest_view_watchers_.rbegin();
       it != guest_view_watchers_.rend(); ++it) {
    const auto& watcher = *it;
    if (!watcher->IsDeleted()) {
      return GetCurrentGuestMainRenderFrameHost(watcher->GetFrameTreeNodeId());
    }
  }
  return nullptr;
}

content::WebContents* TestGuestViewManager::DeprecatedGetLastGuestCreated() {
  return content::WebContents::FromRenderFrameHost(
      GetLastGuestRenderFrameHostCreated());
}

GuestViewBase* TestGuestViewManager::GetLastGuestViewCreated() {
  auto* last_guest = DeprecatedGetLastGuestCreated();
  return GuestViewBase::FromWebContents(last_guest);
}

void TestGuestViewManager::WaitForAllGuestsDeleted() {
  // Make sure that every guest that was created has been removed.
  for (auto& watcher : guest_view_watchers_) {
    watcher->Wait();
  }
}

void TestGuestViewManager::WaitForFirstGuestDeleted() {
  // Wait for the first guest that was created to be deleted.
  guest_view_watchers_.front()->Wait();
}

void TestGuestViewManager::WaitForLastGuestDeleted() {
  // Wait for the last guest that was created to be deleted.
  guest_view_watchers_.back()->Wait();
}

content::RenderFrameHost*
TestGuestViewManager::WaitForSingleGuestRenderFrameHostCreated() {
  if (!GetCurrentGuestCount()) {
    // Guests have been created and subsequently destroyed.
    if (num_guests_created() > 0)
      return nullptr;
    WaitForNumGuestsCreated(1u);
  }
  return GetLastGuestRenderFrameHostCreated();
}

content::WebContents*
TestGuestViewManager::DeprecatedWaitForSingleGuestCreated() {
  return content::WebContents::FromRenderFrameHost(
      WaitForSingleGuestRenderFrameHostCreated());
}

GuestViewBase* TestGuestViewManager::WaitForSingleGuestViewCreated() {
  return GuestViewBase::FromWebContents(DeprecatedWaitForSingleGuestCreated());
}

content::RenderFrameHost*
TestGuestViewManager::WaitForNextGuestRenderFrameHostCreated() {
  created_run_loop_ = std::make_unique<base::RunLoop>();
  created_run_loop_->Run();
  return GetLastGuestRenderFrameHostCreated();
}

content::WebContents*
TestGuestViewManager::DeprecatedWaitForNextGuestCreated() {
  return content::WebContents::FromRenderFrameHost(
      WaitForNextGuestRenderFrameHostCreated());
}

GuestViewBase* TestGuestViewManager::WaitForNextGuestViewCreated() {
  return GuestViewBase::FromWebContents(DeprecatedWaitForNextGuestCreated());
}

void TestGuestViewManager::WaitForNumGuestsCreated(size_t count) {
  if (count == num_guests_created_) {
    return;
  }

  expected_num_guests_created_ = count;

  num_created_run_loop_ = std::make_unique<base::RunLoop>();
  num_created_run_loop_->Run();
  num_created_run_loop_ = nullptr;
}

void TestGuestViewManager::WaitUntilAttached(GuestViewBase* guest_view) {
  if (guest_view->attached())
    return;

  instance_waiting_for_attach_ = guest_view->guest_instance_id();

  attached_run_loop_ = std::make_unique<base::RunLoop>();
  attached_run_loop_->Run();

  // Completion of the attachment process may be delayed despite AttachGuest
  // having been called. We need to wait until the attachment is no longer
  // considered in progress.
  while (!guest_view->attached()) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

void TestGuestViewManager::WaitForViewGarbageCollected() {
  gc_run_loop_ = std::make_unique<base::RunLoop>();
  gc_run_loop_->Run();
}

void TestGuestViewManager::WaitForSingleViewGarbageCollected() {
  if (!num_views_garbage_collected())
    WaitForViewGarbageCollected();
}

void TestGuestViewManager::AddGuest(GuestViewBase* guest) {
  GuestViewManager::AddGuest(guest);

  guest_view_watchers_.push_back(
      std::make_unique<content::FrameDeletedObserver>(
          guest->GetGuestMainFrame()));

  if (created_run_loop_)
    created_run_loop_->Quit();

  ++num_guests_created_;

  if (num_created_run_loop_ &&
      num_guests_created_ == expected_num_guests_created_) {
    num_created_run_loop_->Quit();
  }
}

void TestGuestViewManager::AttachGuest(int embedder_process_id,
                                       int element_instance_id,
                                       int guest_instance_id,
                                       const base::Value::Dict& attach_params) {
  auto* guest_to_attach =
      GuestViewBase::FromInstanceID(embedder_process_id, guest_instance_id);
  if (will_attach_callback_)
    std::move(will_attach_callback_).Run(guest_to_attach);

  GuestViewManager::AttachGuest(embedder_process_id, element_instance_id,
                                guest_instance_id, attach_params);

  if (instance_waiting_for_attach_ == guest_instance_id) {
    CHECK_NE(instance_waiting_for_attach_, kInstanceIDNone);
    attached_run_loop_->Quit();
    instance_waiting_for_attach_ = kInstanceIDNone;
  }
}

void TestGuestViewManager::GetGuestRenderFrameHostList(
    std::vector<content::RenderFrameHost*>* guest_render_frame_host_list) {
  for (auto& watcher : guest_view_watchers_) {
    if (!watcher->IsDeleted()) {
      guest_render_frame_host_list->push_back(
          GetCurrentGuestMainRenderFrameHost(watcher->GetFrameTreeNodeId()));
    }
  }
}

void TestGuestViewManager::EmbedderProcessDestroyed(int embedder_process_id) {
  ++num_embedder_processes_destroyed_;
  GuestViewManager::EmbedderProcessDestroyed(embedder_process_id);
}

void TestGuestViewManager::ViewGarbageCollected(int embedder_process_id,
                                                int view_instance_id) {
  GuestViewManager::ViewGarbageCollected(embedder_process_id, view_instance_id);
  ++num_views_garbage_collected_;
  if (gc_run_loop_)
    gc_run_loop_->Quit();
}

// Test factory for creating test instances of GuestViewManager.
TestGuestViewManagerFactory::TestGuestViewManagerFactory() {
  GuestViewManager::set_factory_for_testing(this);
}

TestGuestViewManagerFactory::~TestGuestViewManagerFactory() {
  GuestViewManager::set_factory_for_testing(nullptr);
}

TestGuestViewManager*
TestGuestViewManagerFactory::GetOrCreateTestGuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate) {
  GuestViewManager* manager = GuestViewManager::FromBrowserContext(context);

  // Test code may access the TestGuestViewManager before it would be created
  // during creation of the first guest.
  if (!manager) {
    manager =
        GuestViewManager::CreateWithDelegate(context, std::move(delegate));
  }

  return static_cast<TestGuestViewManager*>(manager);
}

std::unique_ptr<GuestViewManager>
TestGuestViewManagerFactory::CreateGuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate) {
  return std::make_unique<TestGuestViewManager>(context, std::move(delegate));
}

}  // namespace guest_view
