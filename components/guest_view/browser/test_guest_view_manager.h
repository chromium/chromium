// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"

namespace guest_view {

class TestGuestViewManager : public GuestViewManager {
 public:
  TestGuestViewManager(content::BrowserContext* context,
                       std::unique_ptr<GuestViewManagerDelegate> delegate);

  TestGuestViewManager(const TestGuestViewManager&) = delete;
  TestGuestViewManager& operator=(const TestGuestViewManager&) = delete;

  ~TestGuestViewManager() override;

  void WaitForAllGuestsDeleted();
  void WaitForFirstGuestDeleted();
  void WaitForLastGuestDeleted();

  // While the GuestViewBase directly represents a guest view, the
  // RenderFrameHost version exposes the guest view's main frame for the ease of
  // testing.
  //
  // All the WebContents versions APIs (here and on) will be removed during the
  // MPArch migration. Consider using GuestViewBase or RenderFrameHost versions,
  // unless necessary.
  //
  // TODO(crbug.com/1261928): Remove all the WebContents version.
  GuestViewBase* WaitForSingleGuestViewCreated();
  content::RenderFrameHost* WaitForSingleGuestRenderFrameHostCreated();
  content::WebContents* DeprecatedWaitForSingleGuestCreated();

  GuestViewBase* WaitForNextGuestViewCreated();
  content::RenderFrameHost* WaitForNextGuestRenderFrameHostCreated();
  content::WebContents* DeprecatedWaitForNextGuestCreated();

  void WaitForNumGuestsCreated(size_t count);

  void WaitForSingleViewGarbageCollected();

  GuestViewBase* GetLastGuestViewCreated();
  content::RenderFrameHost* GetLastGuestRenderFrameHostCreated();
  content::WebContents* DeprecatedGetLastGuestCreated();

  void WaitUntilAttached(GuestViewBase* guest_view);

  // Returns the number of guests currently still alive at the time of calling
  // this method.
  size_t GetCurrentGuestCount() const;

  // Returns the size of the set of removed instance IDs.
  size_t GetNumRemovedInstanceIDs() const;

  // Returns the number of times EmbedderWillBeDestroyed() was called.
  int num_embedder_processes_destroyed() const {
    return num_embedder_processes_destroyed_;
  }

  // Returns the number of guests that have been created since the creation of
  // this GuestViewManager.
  size_t num_guests_created() const { return num_guests_created_; }

  // Returns the number of GuestViews that have been garbage collected in
  // JavaScript since the creation of this GuestViewManager.
  int num_views_garbage_collected() const {
    return num_views_garbage_collected_;
  }

  // Returns the last guest instance ID removed from the manager.
  int last_instance_id_removed() const { return last_instance_id_removed_; }

  // Returns the list of guests that were created by this manager.
  void GetGuestRenderFrameHostList(
      std::vector<content::RenderFrameHost*>* guest_render_frame_host_list);

  void SetWillAttachCallback(
      base::OnceCallback<void(GuestViewBase*)> callback) {
    // The callback will be called when the guest view has been created but is
    // not yet attached to the outer.
    will_attach_callback_ = std::move(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, AddRemove);
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, ReuseIdForRecreatedGuestPage);

  // guest_view::GuestViewManager:
  void AddGuest(GuestViewBase* guest) override;
  void EmbedderProcessDestroyed(int embedder_process_id) override;
  void ViewGarbageCollected(int embedder_process_id,
                            int view_instance_id) override;
  void AttachGuest(int embedder_process_id,
                   int element_instance_id,
                   int guest_instance_id,
                   const base::Value::Dict& attach_params) override;

  void WaitForViewGarbageCollected();

  using GuestViewManager::last_instance_id_removed_;
  using GuestViewManager::removed_instance_ids_;

  int num_embedder_processes_destroyed_;
  size_t num_guests_created_;
  size_t expected_num_guests_created_;
  int num_views_garbage_collected_;
  bool waiting_for_guests_created_;

  // Tracks the life time of the GuestView's main FrameTreeNode. The main FTN
  // has the same lifesspan as the GuestView.
  std::vector<std::unique_ptr<content::FrameDeletedObserver>>
      guest_view_watchers_;
  std::unique_ptr<base::RunLoop> created_run_loop_;
  std::unique_ptr<base::RunLoop> num_created_run_loop_;
  raw_ptr<GuestViewBase, DanglingUntriaged> waiting_for_attach_;
  std::unique_ptr<base::RunLoop> attached_run_loop_;
  std::unique_ptr<base::RunLoop> gc_run_loop_;
  base::OnceCallback<void(GuestViewBase*)> will_attach_callback_;
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory();

  TestGuestViewManagerFactory(const TestGuestViewManagerFactory&) = delete;
  TestGuestViewManagerFactory& operator=(const TestGuestViewManagerFactory&) =
      delete;

  ~TestGuestViewManagerFactory() override;

  GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context,
      std::unique_ptr<GuestViewManagerDelegate> delegate) override;

 private:
  raw_ptr<TestGuestViewManager, DanglingUntriaged> test_guest_view_manager_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_
