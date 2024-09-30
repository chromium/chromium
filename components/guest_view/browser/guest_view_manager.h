// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/web_contents.h"

namespace content {
class BrowserContext;
class SiteInstance;
class StoragePartitionConfig;
}

namespace guest_view {

class GuestViewBase;
class GuestViewManagerDelegate;
class GuestViewManagerFactory;

class GuestViewManager : public content::BrowserPluginGuestManager,
                         public base::SupportsUserData::Data {
 public:
  GuestViewManager(content::BrowserContext* context,
                   std::unique_ptr<GuestViewManagerDelegate> delegate);

  GuestViewManager(const GuestViewManager&) = delete;
  GuestViewManager& operator=(const GuestViewManager&) = delete;

  ~GuestViewManager() override;

  // Returns the GuestViewManager associated with |context|. If one isn't
  // available, then it is created and returned.
  static GuestViewManager* CreateWithDelegate(
      content::BrowserContext* context,
      std::unique_ptr<GuestViewManagerDelegate> delegate);

  // Returns the GuestViewManager associated with |context|. If one isn't
  // available, then nullptr is returned.
  static GuestViewManager* FromBrowserContext(content::BrowserContext* context);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(GuestViewManagerFactory* factory);

  // Returns the guest associated with the given |guest_instance_id|
  // if the provided |embedder_render_process_id| is allowed to access it.
  // If the embedder is not allowed access, the embedder will be killed, and
  // this method will return NULL. If no guest exists with the given
  // instance ID, then NULL will also be returned.
  GuestViewBase* GetGuestByInstanceIDSafely(int guest_instance_id,
                                            int embedder_render_process_id);

  // Associates the Browser Plugin with |element_instance_id| to a
  // guest that has ID of |guest_instance_id| and sets initialization
  // parameters, |params| for it.
  virtual void AttachGuest(int embedder_process_id,
                           int element_instance_id,
                           int guest_instance_id,
                           const base::Value::Dict& attach_params);

  // Indicates whether the |guest| is owned by an extension or Chrome App.
  bool IsOwnedByExtension(const GuestViewBase* guest);

  // Indicates whether the |guest| is owned by a Controlled Frame embedder.
  bool IsOwnedByControlledFrameEmbedder(const GuestViewBase* guest);

  int GetNextInstanceID();

  base::WeakPtr<GuestViewManager> AsWeakPtr();

  using GuestViewCreateFunction =
      base::RepeatingCallback<std::unique_ptr<GuestViewBase>(
          content::RenderFrameHost* owner_rfh)>;
  using GuestViewCleanUpFunction =
      base::RepeatingCallback<void(content::BrowserContext*,
                                   int embedder_process_id,
                                   int view_instance_id)>;
  void RegisterGuestViewType(const std::string& type,
                             GuestViewCreateFunction create_function,
                             GuestViewCleanUpFunction cleanup_function);

  // Registers a callback to be called when the view identified by
  // |embedder_process_id| and |view_instance_id| is destroyed.
  // Note that multiple callbacks can be registered for one view.
  void RegisterViewDestructionCallback(int embedder_process_id,
                                       int view_instance_id,
                                       base::OnceClosure callback);

  using UnownedGuestCreatedCallback = base::OnceCallback<void(GuestViewBase*)>;
  using OwnedGuestCreatedCallback =
      base::OnceCallback<void(std::unique_ptr<GuestViewBase>)>;
  // Creates a guest and has the GuestViewManager assume ownership.
  void CreateGuest(const std::string& view_type,
                   content::RenderFrameHost* owner_rfh,
                   const base::Value::Dict& create_params,
                   UnownedGuestCreatedCallback callback);
  // Creates a guest which the caller will own.
  void CreateGuestAndTransferOwnership(const std::string& view_type,
                                       content::RenderFrameHost* owner_rfh,
                                       const base::Value::Dict& create_params,
                                       OwnedGuestCreatedCallback callback);

  // Transfers ownership of `guest` to the caller.
  std::unique_ptr<GuestViewBase> TransferOwnership(GuestViewBase* guest);
  // Have `this` manage ownership of `guest`.
  void ManageOwnership(std::unique_ptr<GuestViewBase> guest);

  std::unique_ptr<content::WebContents> CreateGuestWithWebContentsParams(
      const std::string& view_type,
      content::RenderFrameHost* owner_rfh,
      const content::WebContents::CreateParams& create_params);

  content::SiteInstance* GetGuestSiteInstance(
      const content::StoragePartitionConfig& storage_partition_config);

  // BrowserPluginGuestManager implementation.
  void ForEachUnattachedGuest(
      content::WebContents* owner_web_contents,
      base::FunctionRef<void(content::WebContents*)> fn) override;
  bool ForEachGuest(content::WebContents* owner_web_contents,
                    base::FunctionRef<bool(content::WebContents*)> fn) override;
  content::WebContents* GetFullPageGuest(
      content::WebContents* embedder_web_contents) override;

 protected:
  friend class GuestViewBase;
  friend class GuestViewEvent;
  friend class GuestViewMessageHandler;
  friend class ViewHandle;

  class EmbedderRenderProcessHostObserver;

  // These methods are virtual so that they can be overriden in tests.

  virtual void AddGuest(GuestViewBase* guest);
  // If a GuestView is created but never initialized with a guest WebContents,
  // this should still be called to invalidate `guest`'s `guest_instance_id`.
  // If `invalidate_id` is false, then the id may be reused to associate a guest
  // with a new guest WebContents.
  void RemoveGuest(GuestViewBase* guest, bool invalidate_id);

  GuestViewBase* GetGuestFromWebContents(content::WebContents* web_contents);

  // This method is called when the embedder process with ID
  // |embedder_process_id| has been destroyed.
  virtual void EmbedderProcessDestroyed(int embedder_process_id);

  // Called when a GuestView has been created in JavaScript.
  virtual void ViewCreated(int embedder_process_id,
                           int view_instance_id,
                           const std::string& view_type);

  // Called when a GuestView has been garbage collected in JavaScript.
  virtual void ViewGarbageCollected(int embedder_process_id,
                                    int view_instance_id);

  // Calls all destruction callbacks registered for the GuestView identified by
  // |embedder_process_id| and |view_instance_id|.
  void CallViewDestructionCallbacks(int embedder_process_id,
                                    int view_instance_id);

  // Calls all destruction callbacks registered for GuestViews in the embedder
  // with ID |embedder_process_id|.
  void CallViewDestructionCallbacks(int embedder_process_id);

  // Creates a guest of the provided |view_type|.
  std::unique_ptr<GuestViewBase> CreateGuestInternal(
      content::RenderFrameHost* owner_rfh,
      const std::string& view_type);

  // Adds GuestView types to the GuestView registry.
  void RegisterGuestViewTypes();

  // Starts observing an embedder process's lifetime.
  void ObserveEmbedderLifetime(content::RenderProcessHost* embedder_process);

  // Indicates whether the provided |guest| can be used in the context it has
  // been created.
  bool IsGuestAvailableToContext(GuestViewBase* guest);

  // Dispatches the event with |name| with the provided |args| to the embedder
  // of the given |guest| with |instance_id| for routing.
  void DispatchEvent(const std::string& event_name,
                     base::Value::Dict args,
                     GuestViewBase* guest,
                     int instance_id);

  GuestViewBase* GetGuestByInstanceID(int guest_instance_id);

  bool CanEmbedderAccessInstanceIDMaybeKill(
      int embedder_render_process_id,
      int guest_instance_id);

  bool CanEmbedderAccessInstanceID(int embedder_render_process_id,
                                   int guest_instance_id);

  // Returns true if |guest_instance_id| can be used to add a new guest to this
  // manager.
  // We disallow adding new guest with instance IDs that were previously removed
  // from this manager using RemoveGuest.
  bool CanUseGuestInstanceID(int guest_instance_id);

  // Contains guests, mapping from their instance ids.
  using GuestInstanceMap =
      std::map<int, raw_ptr<GuestViewBase, CtnExperimental>>;
  GuestInstanceMap guests_by_instance_id_;

  using WebContentsGuestViewMap =
      std::map<const content::WebContents*,
               raw_ptr<GuestViewBase, CtnExperimental>>;
  WebContentsGuestViewMap webcontents_guestview_map_;

  struct ElementInstanceKey {
    int embedder_process_id;
    int element_instance_id;

    ElementInstanceKey();
    ElementInstanceKey(int embedder_process_id,
                       int element_instance_id);

    bool operator<(const ElementInstanceKey& other) const;
  };

  using GuestInstanceIDMap = std::map<ElementInstanceKey, int>;
  GuestInstanceIDMap instance_id_map_;

  // The reverse map of GuestInstanceIDMap.
  using GuestInstanceIDReverseMap = std::map<int, ElementInstanceKey>;
  GuestInstanceIDReverseMap reverse_instance_id_map_;

  struct GuestViewData {
    GuestViewData(const GuestViewCreateFunction& create_function,
                  const GuestViewCleanUpFunction& cleanup_function);
    GuestViewData(const GuestViewData& other);
    ~GuestViewData();
    const GuestViewCreateFunction create_function;
    const GuestViewCleanUpFunction cleanup_function;
  };
  using GuestViewMethodMap = std::map<std::string, GuestViewData>;
  GuestViewMethodMap guest_view_registry_;

  int current_instance_id_ = 0;

  // Any instance ID whose number not greater than this was removed via
  // RemoveGuest.
  // This is used so that we don't have store all removed instance IDs in
  // |removed_instance_ids_|.
  int last_instance_id_removed_ = 0;
  // The remaining instance IDs that are greater than
  // |last_instance_id_removed_| are kept here.
  std::set<int> removed_instance_ids_;

  const raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<GuestViewManagerDelegate> delegate_;

  // This tracks which GuestView embedders are currently being observed.
  std::set<int> embedders_observed_;

  // Maps embedder process ids to unattached guests whose lifetimes are being
  // managed by this GuestViewManager. An unattached guest's lifetime is scoped
  // to the process that created it by this manager. Ownership is taken from
  // this manager via `TransferOwnership` upon guest attachment, or for cases
  // where an unattached guest needs to be destroyed earlier.
  std::multimap<int, std::unique_ptr<GuestViewBase>> owned_guests_;

  // |view_destruction_callback_map_| maps from embedder process ID to view ID
  // to a vector of callback functions to be called when that view is destroyed.
  using Callbacks = std::vector<base::OnceClosure>;
  using CallbacksForEachViewID = std::map<int, Callbacks>;
  using CallbacksForEachEmbedderID = std::map<int, CallbacksForEachViewID>;
  CallbacksForEachEmbedderID view_destruction_callback_map_;

  base::WeakPtrFactory<GuestViewManager> weak_ptr_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_H_
