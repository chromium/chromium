// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"

#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserContext;
using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

namespace guest_view {

namespace {

// Static factory instance (always NULL for non-test).
GuestViewManagerFactory* g_factory;

}  // namespace

// This observer observes the RenderProcessHosts of GuestView embedders, and
// notifies the GuestViewManager when they are destroyed.
class GuestViewManager::EmbedderRenderProcessHostObserver
    : public content::RenderProcessHostObserver {
 public:
  EmbedderRenderProcessHostObserver(
      base::WeakPtr<GuestViewManager> guest_view_manager,
      RenderProcessHost* host)
      : guest_view_manager_(guest_view_manager) {
    DCHECK(host);
    host->AddObserver(this);
  }

  void RenderProcessExited(
      RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    if (guest_view_manager_)
      guest_view_manager_->EmbedderProcessDestroyed(host->GetID());
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    host->RemoveObserver(this);
    delete this;
  }

 private:
  base::WeakPtr<GuestViewManager> guest_view_manager_;
};

GuestViewManager::GuestViewManager(
    content::BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate)
    : context_(context), delegate_(std::move(delegate)) {}

GuestViewManager::~GuestViewManager() {
  // It seems that ChromeOS OTR profiles may still have RenderProcessHosts at
  // this point. See https://crbug.com/828479
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(view_destruction_callback_map_.empty());
#endif
}

// static
GuestViewManager* GuestViewManager::CreateWithDelegate(
    BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate) {
  GuestViewManager* guest_manager = FromBrowserContext(context);
  if (!guest_manager) {
    std::unique_ptr<GuestViewManager> new_manager =
        g_factory
            ? g_factory->CreateGuestViewManager(context, std::move(delegate))
            : std::make_unique<GuestViewManager>(context, std::move(delegate));
    guest_manager = new_manager.get();
    context->SetUserData(kGuestViewManagerKeyName, std::move(new_manager));
  }
  return guest_manager;
}

// static
GuestViewManager* GuestViewManager::FromBrowserContext(
    BrowserContext* context) {
  return static_cast<GuestViewManager*>(context->GetUserData(
      kGuestViewManagerKeyName));
}

// static
void GuestViewManager::set_factory_for_testing(
    GuestViewManagerFactory* factory) {
  g_factory = factory;
}

GuestViewBase* GuestViewManager::GetGuestByInstanceIDSafely(
    int guest_instance_id,
    int embedder_render_process_id) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    return nullptr;
  }
  return GetGuestByInstanceID(guest_instance_id);
}

void GuestViewManager::AttachGuest(int embedder_process_id,
                                   int element_instance_id,
                                   int guest_instance_id,
                                   const base::Value::Dict& attach_params) {
  auto* guest_view =
      GuestViewBase::FromInstanceID(embedder_process_id, guest_instance_id);
  if (!guest_view)
    return;

  ElementInstanceKey key(embedder_process_id, element_instance_id);

  // If there is an existing guest attached to the element, then the embedder is
  // misbehaving.
  if (base::Contains(instance_id_map_, key)) {
    bad_message::ReceivedBadMessage(embedder_process_id,
                                    bad_message::GVM_INVALID_ATTACH);
    return;
  }

  instance_id_map_[key] = guest_instance_id;
  reverse_instance_id_map_[guest_instance_id] = key;
  guest_view->SetAttachParams(attach_params);
}

bool GuestViewManager::IsOwnedByExtension(const GuestViewBase* guest) {
  return delegate_->IsOwnedByExtension(guest);
}

bool GuestViewManager::IsOwnedByControlledFrameEmbedder(
    const GuestViewBase* guest) {
  return delegate_->IsOwnedByControlledFrameEmbedder(guest);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

base::WeakPtr<GuestViewManager> GuestViewManager::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GuestViewManager::CreateGuest(const std::string& view_type,
                                   content::RenderFrameHost* owner_rfh,
                                   const base::Value::Dict& create_params,
                                   UnownedGuestCreatedCallback callback) {
  OwnedGuestCreatedCallback ownership_transferring_callback = base::BindOnce(
      [](UnownedGuestCreatedCallback callback,
         std::unique_ptr<GuestViewBase> guest) {
        auto* raw_guest = guest.get();
        if (raw_guest) {
          raw_guest->GetGuestViewManager()->ManageOwnership(std::move(guest));
        }
        std::move(callback).Run(raw_guest);
      },
      std::move(callback));
  CreateGuestAndTransferOwnership(view_type, owner_rfh, create_params,
                                  std::move(ownership_transferring_callback));
}

void GuestViewManager::CreateGuestAndTransferOwnership(
    const std::string& view_type,
    content::RenderFrameHost* owner_rfh,
    const base::Value::Dict& create_params,
    OwnedGuestCreatedCallback callback) {
  std::unique_ptr<GuestViewBase> guest =
      CreateGuestInternal(owner_rfh, view_type);
  if (!guest) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto* raw_guest = guest.get();
  raw_guest->Init(std::move(guest), create_params, std::move(callback));
}

std::unique_ptr<GuestViewBase> GuestViewManager::TransferOwnership(
    GuestViewBase* guest) {
  for (auto it = owned_guests_.begin(); it != owned_guests_.end(); ++it) {
    if (it->second.get() == guest) {
      std::unique_ptr<GuestViewBase> owned_guest = std::move(it->second);
      owned_guests_.erase(it);
      return owned_guest;
    }
  }

  return nullptr;
}

void GuestViewManager::ManageOwnership(std::unique_ptr<GuestViewBase> guest) {
  RenderProcessHost* owner_process = guest->owner_rfh()->GetProcess();
  DCHECK(owner_process);
  ObserveEmbedderLifetime(owner_process);
  owned_guests_.insert({owner_process->GetID(), std::move(guest)});
}

std::unique_ptr<content::WebContents>
GuestViewManager::CreateGuestWithWebContentsParams(
    const std::string& view_type,
    content::RenderFrameHost* owner_rfh,
    const content::WebContents::CreateParams& create_params) {
  std::unique_ptr<GuestViewBase> guest =
      CreateGuestInternal(owner_rfh, view_type);
  if (!guest)
    return nullptr;

  // TODO(crbug.com/40254126): For the noopener case, it would be better to
  // delay the creation of the guest contents until attachment.
  content::WebContents::CreateParams guest_create_params(create_params);
  guest_create_params.guest_delegate = guest.get();

  std::unique_ptr<content::WebContents> guest_web_contents =
      WebContents::Create(guest_create_params);
  const base::Value::Dict guest_params = base::Value::Dict();
  guest->SetCreateParams(guest_params, guest_create_params);
  guest->InitWithWebContents(guest_params, guest_web_contents.get());
  ManageOwnership(std::move(guest));
  // Ownership of the guest WebContents goes to the content layer until we get
  // it back in AddNewContents.
  return guest_web_contents;
}

SiteInstance* GuestViewManager::GetGuestSiteInstance(
    const content::StoragePartitionConfig& storage_partition_config) {
  for (auto [id, guest] : guests_by_instance_id_) {
    content::RenderFrameHost* guest_main_frame = guest->GetGuestMainFrame();
    if (guest_main_frame &&
        guest_main_frame->GetSiteInstance()->GetStoragePartitionConfig() ==
            storage_partition_config) {
      return guest_main_frame->GetSiteInstance();
    }
  }
  return nullptr;
}

void GuestViewManager::ForEachUnattachedGuest(
    content::WebContents* owner_web_contents,
    base::FunctionRef<void(content::WebContents*)> fn) {
  for (auto [id, guest] : guests_by_instance_id_) {
    if (guest->owner_web_contents() == owner_web_contents &&
        !guest->attached() && guest->web_contents()) {
      fn(guest->web_contents());
    }
  }
}

bool GuestViewManager::ForEachGuest(
    WebContents* owner_web_contents,
    base::FunctionRef<bool(content::WebContents*)> fn) {
  for (auto [id, guest] : guests_by_instance_id_) {
    if (!guest->web_contents() ||
        guest->owner_web_contents() != owner_web_contents) {
      continue;
    }

    if (fn(guest->web_contents())) {
      return true;
    }
  }
  return false;
}

WebContents* GuestViewManager::GetFullPageGuest(
    WebContents* embedder_web_contents) {
  WebContents* result = nullptr;
  ForEachGuest(
      embedder_web_contents, [&](content::WebContents* guest_web_contents) {
        auto* guest_view = GuestViewBase::FromWebContents(guest_web_contents);
        if (guest_view && guest_view->is_full_page_plugin()) {
          result = guest_web_contents;
          return true;
        }
        return false;
      });

  return result;
}

void GuestViewManager::AddGuest(GuestViewBase* guest) {
  const int guest_instance_id = guest->guest_instance_id();
  WebContents* guest_web_contents = guest->web_contents();

  CHECK(CanUseGuestInstanceID(guest_instance_id));
  const auto [it, success] =
      guests_by_instance_id_.insert({guest_instance_id, guest});
  // The guest may already be tracked if we needed to recreate the guest
  // contents. In that case, the ID must still refer to the same guest when
  // re-adding it here.
  CHECK(success || it->second == guest);

  webcontents_guestview_map_.insert({guest_web_contents, guest});

  delegate_->OnGuestAdded(guest_web_contents);
}

void GuestViewManager::RemoveGuest(GuestViewBase* guest, bool invalidate_id) {
  const int guest_instance_id = guest->guest_instance_id();

  webcontents_guestview_map_.erase(guest->web_contents());

  auto id_iter = reverse_instance_id_map_.find(guest_instance_id);
  if (id_iter != reverse_instance_id_map_.end()) {
    const ElementInstanceKey& instance_id_key = id_iter->second;
    instance_id_map_.erase(instance_id_key);
    reverse_instance_id_map_.erase(id_iter);
  }

  if (!invalidate_id) {
    return;
  }

  // We don't stop tracking the `guest` when `invalidate_id` is false as the
  // guest still exists and may go on to recreate its guest contents.
  guests_by_instance_id_.erase(guest_instance_id);

  // All the instance IDs that lie within [0, last_instance_id_removed_]
  // are invalid.
  // The remaining sparse invalid IDs are kept in |removed_instance_ids_| set.
  // The following code compacts the set by incrementing
  // |last_instance_id_removed_|.
  if (guest_instance_id == last_instance_id_removed_ + 1) {
    ++last_instance_id_removed_;
    // Compact.
    auto iter = removed_instance_ids_.begin();
    while (iter != removed_instance_ids_.end()) {
      int instance_id = *iter;
      // The sparse invalid IDs must not lie within
      // [0, last_instance_id_removed_]
      DCHECK_GT(instance_id, last_instance_id_removed_);
      if (instance_id != last_instance_id_removed_ + 1)
        break;
      ++last_instance_id_removed_;
      removed_instance_ids_.erase(iter++);
    }
  } else if (guest_instance_id > last_instance_id_removed_) {
    removed_instance_ids_.insert(guest_instance_id);
  }
}

GuestViewBase* GuestViewManager::GetGuestFromWebContents(
    content::WebContents* web_contents) {
  auto it = webcontents_guestview_map_.find(web_contents);
  return it == webcontents_guestview_map_.end() ? nullptr : it->second;
}

void GuestViewManager::EmbedderProcessDestroyed(int embedder_process_id) {
  embedders_observed_.erase(embedder_process_id);

  // We can't just call std::multimap::erase here because destroying a guest
  // could trigger the destruction of another guest which is also owned by
  // `owned_guests_`. Recursively calling std::multimap::erase is unsafe (see
  // https://crbug.com/1450397). So we take ownership of all of the guests that
  // will be destroyed before erasing the entries from the map.
  std::vector<std::unique_ptr<GuestViewBase>> guests_to_destroy;
  const auto destroy_range = owned_guests_.equal_range(embedder_process_id);
  for (auto it = destroy_range.first; it != destroy_range.second; ++it) {
    guests_to_destroy.push_back(std::move(it->second));
  }
  owned_guests_.erase(embedder_process_id);
  guests_to_destroy.clear();

  CallViewDestructionCallbacks(embedder_process_id);
}

void GuestViewManager::ViewCreated(int embedder_process_id,
                                   int view_instance_id,
                                   const std::string& view_type) {
  if (guest_view_registry_.empty())
    RegisterGuestViewTypes();
  auto view_it = guest_view_registry_.find(view_type);
  if (view_it == guest_view_registry_.end()) {
    bad_message::ReceivedBadMessage(embedder_process_id,
                                    bad_message::GVM_INVALID_GUESTVIEW_TYPE);
    return;
  }

  // Register the cleanup callback for when this view is destroyed.
  if (view_it->second.cleanup_function) {
    RegisterViewDestructionCallback(
        embedder_process_id, view_instance_id,
        base::BindOnce(view_it->second.cleanup_function, context_,
                       embedder_process_id, view_instance_id));
  }
}

void GuestViewManager::ViewGarbageCollected(int embedder_process_id,
                                            int view_instance_id) {
  CallViewDestructionCallbacks(embedder_process_id, view_instance_id);
}

void GuestViewManager::CallViewDestructionCallbacks(int embedder_process_id,
                                                    int view_instance_id) {
  // Find the callbacks for the embedder with ID |embedder_process_id|.
  auto embedder_it = view_destruction_callback_map_.find(embedder_process_id);
  if (embedder_it == view_destruction_callback_map_.end())
    return;
  auto& callbacks_for_embedder = embedder_it->second;

  // If |view_instance_id| is guest_view::kInstanceIDNone, then all callbacks
  // for this embedder should be called.
  if (view_instance_id == kInstanceIDNone) {
    // Call all callbacks for the embedder with ID |embedder_process_id|.
    for (auto& view_pair : callbacks_for_embedder) {
      auto& callbacks_for_view = view_pair.second;
      for (auto& callback : callbacks_for_view)
        std::move(callback).Run();
    }
    view_destruction_callback_map_.erase(embedder_it);
    return;
  }

  // Otherwise, call the callbacks only for the specific view with ID
  // |view_instance_id|.
  auto view_it = callbacks_for_embedder.find(view_instance_id);
  if (view_it == callbacks_for_embedder.end())
    return;
  auto& callbacks_for_view = view_it->second;
  for (auto& callback : callbacks_for_view)
    std::move(callback).Run();
  callbacks_for_embedder.erase(view_it);
}

void GuestViewManager::CallViewDestructionCallbacks(int embedder_process_id) {
  CallViewDestructionCallbacks(embedder_process_id, kInstanceIDNone);
}

std::unique_ptr<GuestViewBase> GuestViewManager::CreateGuestInternal(
    content::RenderFrameHost* owner_rfh,
    const std::string& view_type) {
  if (guest_view_registry_.empty())
    RegisterGuestViewTypes();

  auto it = guest_view_registry_.find(view_type);
  if (it == guest_view_registry_.end()) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  return it->second.create_function.Run(owner_rfh);
}

void GuestViewManager::RegisterGuestViewTypes() {
  delegate_->RegisterAdditionalGuestViewTypes(this);
}

void GuestViewManager::RegisterGuestViewType(
    const std::string& type,
    GuestViewCreateFunction create_function,
    GuestViewCleanUpFunction cleanup_function) {
  // If the GuestView type `type` is already registered, then there is nothing
  // more to do. If an existing entry in the registry was created by this
  // function for `type`, then registering again would have no effect, and
  // if it was registered elsewhere, then we do not want to overwrite it. Note
  // that it is possible for tests to have special test factory methods
  // registered here.
  if (base::Contains(guest_view_registry_, type))
    return;

  guest_view_registry_.insert({type, {create_function, cleanup_function}});
}

void GuestViewManager::RegisterViewDestructionCallback(
    int embedder_process_id,
    int view_instance_id,
    base::OnceClosure callback) {
  RenderProcessHost* rph = RenderProcessHost::FromID(embedder_process_id);
  // The RenderProcessHost may already be gone.
  if (!rph) {
    std::move(callback).Run();
    return;
  }

  ObserveEmbedderLifetime(rph);

  view_destruction_callback_map_[embedder_process_id][view_instance_id]
      .push_back(std::move(callback));
}

void GuestViewManager::ObserveEmbedderLifetime(
    RenderProcessHost* embedder_process) {
  if (!embedders_observed_.count(embedder_process->GetID())) {
    embedders_observed_.insert(embedder_process->GetID());
    // EmbedderRenderProcessHostObserver owns itself.
    new EmbedderRenderProcessHostObserver(weak_ptr_factory_.GetWeakPtr(),
                                          embedder_process);
  }
}

bool GuestViewManager::IsGuestAvailableToContext(GuestViewBase* guest) {
  return delegate_->IsGuestAvailableToContext(guest);
}

void GuestViewManager::DispatchEvent(const std::string& event_name,
                                     base::Value::Dict args,
                                     GuestViewBase* guest,
                                     int instance_id) {
  // TODO(fsamuel): GuestViewManager should probably do something more useful
  // here like log an error if the event could not be dispatched.
  delegate_->DispatchEvent(event_name, std::move(args), guest, instance_id);
}

GuestViewBase* GuestViewManager::GetGuestByInstanceID(int guest_instance_id) {
  auto it = guests_by_instance_id_.find(guest_instance_id);
  return it == guests_by_instance_id_.end() ? nullptr : it->second;
}

bool GuestViewManager::CanEmbedderAccessInstanceIDMaybeKill(
    int embedder_render_process_id,
    int guest_instance_id) {
  if (!CanEmbedderAccessInstanceID(embedder_render_process_id,
                                   guest_instance_id)) {
    // The embedder process is trying to access a guest it does not own.
    bad_message::ReceivedBadMessage(
        embedder_render_process_id,
        bad_message::GVM_EMBEDDER_FORBIDDEN_ACCESS_TO_GUEST);
    return false;
  }
  return true;
}

bool GuestViewManager::CanUseGuestInstanceID(int guest_instance_id) {
  if (guest_instance_id <= last_instance_id_removed_)
    return false;
  return !base::Contains(removed_instance_ids_, guest_instance_id);
}

bool GuestViewManager::CanEmbedderAccessInstanceID(
    int embedder_render_process_id,
    int guest_instance_id) {
  // TODO(crbug.com/41353094): Remove crash key once the cause of the kill is
  // known.
  static crash_reporter::CrashKeyString<32> bad_access_key("guest-bad-access");

  // The embedder is trying to access a guest with a negative or zero
  // instance ID.
  if (guest_instance_id <= kInstanceIDNone) {
    bad_access_key.Set("Nonpositive");
    return false;
  }

  // The embedder is trying to access an instance ID that has not yet been
  // allocated by GuestViewManager. This could cause instance ID
  // collisions in the future, and potentially give one embedder access to a
  // guest it does not own.
  if (guest_instance_id > current_instance_id_) {
    bad_access_key.Set("Unallocated");
    return false;
  }

  // We might get some late arriving messages at tear down. Let's let the
  // embedder tear down in peace.
  auto* guest_view = GetGuestByInstanceID(guest_instance_id);
  if (!guest_view) {
    return true;
  }

  // MimeHandlerViewGuests (PDF) may be embedded in a cross-process frame.
  // Other than MimeHandlerViewGuest, all other guest types are only permitted
  // to run in the main frame or its local subframes.
  const int allowed_embedder_render_process_id =
      guest_view->CanBeEmbeddedInsideCrossProcessFrames()
          ? guest_view->owner_rfh()->GetProcess()->GetID()
          : guest_view->owner_rfh()->GetMainFrame()->GetProcess()->GetID();

  if (embedder_render_process_id != allowed_embedder_render_process_id) {
    bad_access_key.Set("Bad embedder process");
    return false;
  }

  return true;
}

GuestViewManager::ElementInstanceKey::ElementInstanceKey()
    : embedder_process_id(content::ChildProcessHost::kInvalidUniqueID),
      element_instance_id(kInstanceIDNone) {}

GuestViewManager::ElementInstanceKey::ElementInstanceKey(
    int embedder_process_id,
    int element_instance_id)
    : embedder_process_id(embedder_process_id),
      element_instance_id(element_instance_id) {
}

bool GuestViewManager::ElementInstanceKey::operator<(
    const GuestViewManager::ElementInstanceKey& other) const {
  return std::tie(embedder_process_id, element_instance_id) <
         std::tie(other.embedder_process_id, other.element_instance_id);
}

GuestViewManager::GuestViewData::GuestViewData(
    const GuestViewCreateFunction& create_function,
    const GuestViewCleanUpFunction& cleanup_function)
    : create_function(create_function), cleanup_function(cleanup_function) {}

GuestViewManager::GuestViewData::GuestViewData(const GuestViewData& other) =
    default;

GuestViewManager::GuestViewData::~GuestViewData() = default;

}  // namespace guest_view
