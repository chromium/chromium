// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "content/public/test/test_utils.h"

namespace guest_view {

class TestGuestViewManager : public GuestViewManager {
 public:
  TestGuestViewManager(content::BrowserContext* context,
                       std::unique_ptr<GuestViewManagerDelegate> delegate);
  ~TestGuestViewManager() override;

  void WaitForAllGuestsDeleted();

  void WaitForLastGuestDeleted();

  content::WebContents* WaitForSingleGuestCreated();
  content::WebContents* WaitForNextGuestCreated();
  void WaitForNumGuestsCreated(size_t count);

  void WaitForSingleViewGarbageCollected();

  content::WebContents* GetLastGuestCreated();

  void WaitUntilAttached(content::WebContents* web_contents);

  // Returns the number of guests currently still alive at the time of calling
  // this method.
  size_t GetNumGuestsActive() const;

  // Returns the size of the set of removed instance IDs.
  size_t GetNumRemovedInstanceIDs() const;

  template <typename T>
  void RegisterTestGuestViewType(
      const GuestViewCreateFunction& create_function) {
    auto registry_entry = std::make_pair(
        T::Type,
        GuestViewData(create_function, base::BindRepeating(&T::CleanUp)));
    guest_view_registry_.insert(registry_entry);
  }

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

  // Returns the list of guests WebContentses that were created by this
  // manager.
  void GetGuestWebContentsList(
      std::vector<content::WebContents*>* guest_web_contents_list);

 private:
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, AddRemove);

  // GuestViewManager override:
  void AddGuest(int guest_instance_id,
                content::WebContents* guest_web_contents) override;
  void RemoveGuest(int guest_instance_id) override;
  void EmbedderProcessDestroyed(int embedder_process_id) override;
  void ViewGarbageCollected(int embedder_process_id,
                            int view_instance_id) override;
  void AttachGuest(int embedder_process_id,
                   int element_instance_id,
                   int guest_instance_id,
                   const base::DictionaryValue& attach_params) override;

  void WaitForViewGarbageCollected();

  using GuestViewManager::last_instance_id_removed_;
  using GuestViewManager::removed_instance_ids_;

  int num_embedder_processes_destroyed_;
  size_t num_guests_created_;
  size_t expected_num_guests_created_;
  int num_views_garbage_collected_;
  bool waiting_for_guests_created_;

  std::vector<std::unique_ptr<content::WebContentsDestroyedWatcher>>
      guest_web_contents_watchers_;
  scoped_refptr<content::MessageLoopRunner> created_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> num_created_message_loop_runner_;
  GuestViewBase* waiting_for_attach_;
  scoped_refptr<content::MessageLoopRunner> attached_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> gc_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManager);
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory();
  ~TestGuestViewManagerFactory() override;

  GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context,
      std::unique_ptr<GuestViewManagerDelegate> delegate) override;

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_TEST_GUEST_VIEW_MANAGER_H_
