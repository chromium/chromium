// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"

#include <tuple>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

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
    : current_instance_id_(0),
      last_instance_id_removed_(0),
      context_(context),
      delegate_(std::move(delegate)) {}

GuestViewManager::~GuestViewManager() {
  // It seems that ChromeOS OTR profiles may still have RenderProcessHosts at
  // this point. See https://crbug.com/828479
#if !defined(OS_CHROMEOS)
  DCHECK(view_destruction_callback_map_.empty());
#endif
}

// static
GuestViewManager* GuestViewManager::CreateWithDelegate(
    BrowserContext* context,
    std::unique_ptr<GuestViewManagerDelegate> delegate) {
  GuestViewManager* guest_manager = FromBrowserContext(context);
  if (!guest_manager) {
    if (g_factory) {
      guest_manager =
          g_factory->CreateGuestViewManager(context, std::move(delegate));
    } else {
      guest_manager = new GuestViewManager(context, std::move(delegate));
    }
    context->SetUserData(kGuestViewManagerKeyName,
                         base::WrapUnique(guest_manager));
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

content::WebContents* GuestViewManager::GetGuestByInstanceIDSafely(
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
                                   const base::DictionaryValue& attach_params) {
  auto* guest_view =
      GuestViewBase::From(embedder_process_id, guest_instance_id);
  if (!guest_view)
    return;

  ElementInstanceKey key(embedder_process_id, element_instance_id);
  auto it = instance_id_map_.find(key);
  // If there is an existing guest attached to the element, then destroy the
  // existing guest.
  if (it != instance_id_map_.end()) {
    int old_guest_instance_id = it->second;
    if (old_guest_instance_id == guest_instance_id)
      return;

    auto* old_guest_view =
        GuestViewBase::From(embedder_process_id, old_guest_instance_id);
    old_guest_view->Destroy(true);
  }
  instance_id_map_[key] = guest_instance_id;
  reverse_instance_id_map_[guest_instance_id] = key;
  guest_view->SetAttachParams(attach_params);
}

void GuestViewManager::DetachGuest(GuestViewBase* guest) {
  if (!guest->attached())
    return;

  auto reverse_it = reverse_instance_id_map_.find(guest->guest_instance_id());
  if (reverse_it == reverse_instance_id_map_.end())
    return;

  const ElementInstanceKey& key = reverse_it->second;

  auto it = instance_id_map_.find(key);
  DCHECK(it != instance_id_map_.end());

  reverse_instance_id_map_.erase(reverse_it);
  instance_id_map_.erase(it);
}

bool GuestViewManager::IsOwnedByExtension(GuestViewBase* guest) {
  return delegate_->IsOwnedByExtension(guest);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

void GuestViewManager::CreateGuest(const std::string& view_type,
                                   content::WebContents* owner_web_contents,
                                   const base::DictionaryValue& create_params,
                                   WebContentsCreatedCallback callback) {
  GuestViewBase* guest = CreateGuestInternal(owner_web_contents, view_type);
  if (!guest) {
    std::move(callback).Run(nullptr);
    return;
  }
  guest->Init(create_params, std::move(callback));
}

content::WebContents* GuestViewManager::CreateGuestWithWebContentsParams(
    const std::string& view_type,
    content::WebContents* owner_web_contents,
    const content::WebContents::CreateParams& create_params) {
  auto* guest = CreateGuestInternal(owner_web_contents, view_type);
  if (!guest)
    return nullptr;
  content::WebContents::CreateParams guest_create_params(create_params);
  guest_create_params.guest_delegate = guest;

  // TODO(erikchen): Fix ownership semantics for this class.
  // https://crbug.com/832879.
  std::unique_ptr<content::WebContents> guest_web_contents =
      WebContents::Create(guest_create_params);
  guest->InitWithWebContents(base::DictionaryValue(), guest_web_contents.get());
  return guest_web_contents.release();
}

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int owner_process_id,
    int element_instance_id) {
  int guest_instance_id = GetGuestInstanceIDForElementID(owner_process_id,
                                                         element_instance_id);
  if (guest_instance_id == kInstanceIDNone)
    return nullptr;

  return GetGuestByInstanceID(guest_instance_id);
}

int GuestViewManager::GetGuestInstanceIDForElementID(int owner_process_id,
                                                     int element_instance_id) {
  auto iter = instance_id_map_.find(
      ElementInstanceKey(owner_process_id, element_instance_id));
  if (iter == instance_id_map_.end())
    return kInstanceIDNone;
  return iter->second;
}

SiteInstance* GuestViewManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  for (const auto& guest : guest_web_contents_by_instance_id_) {
    if (guest.second->GetSiteInstance()->GetSiteURL() == guest_site)
      return guest.second->GetSiteInstance();
  }
  return nullptr;
}

bool GuestViewManager::ForEachGuest(WebContents* owner_web_contents,
                                    const GuestCallback& callback) {
  for (const auto& guest : guest_web_contents_by_instance_id_) {
    auto* guest_view = GuestViewBase::FromWebContents(guest.second);
    if (guest_view->owner_web_contents() != owner_web_contents)
      continue;

    if (callback.Run(guest_view->web_contents()))
      return true;
  }
  return false;
}

WebContents* GuestViewManager::GetFullPageGuest(
    WebContents* embedder_web_contents) {
  WebContents* result = nullptr;
  ForEachGuest(embedder_web_contents,
               base::Bind(&GuestViewManager::GetFullPageGuestHelper, &result));
  return result;
}

void GuestViewManager::AddGuest(int guest_instance_id,
                                WebContents* guest_web_contents) {
  CHECK(!base::Contains(guest_web_contents_by_instance_id_, guest_instance_id));
  CHECK(CanUseGuestInstanceID(guest_instance_id));
  guest_web_contents_by_instance_id_[guest_instance_id] = guest_web_contents;

  delegate_->OnGuestAdded(guest_web_contents);
}

void GuestViewManager::RemoveGuest(int guest_instance_id) {
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(it);

  auto id_iter = reverse_instance_id_map_.find(guest_instance_id);
  if (id_iter != reverse_instance_id_map_.end()) {
    const ElementInstanceKey& instance_id_key = id_iter->second;
    instance_id_map_.erase(instance_id_map_.find(instance_id_key));
    reverse_instance_id_map_.erase(id_iter);
  }

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
      DCHECK(instance_id > last_instance_id_removed_);
      if (instance_id != last_instance_id_removed_ + 1)
        break;
      ++last_instance_id_removed_;
      removed_instance_ids_.erase(iter++);
    }
  } else {
    removed_instance_ids_.insert(guest_instance_id);
  }
}

void GuestViewManager::EmbedderProcessDestroyed(int embedder_process_id) {
  embedders_observed_.erase(embedder_process_id);
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
  RegisterViewDestructionCallback(
      embedder_process_id, view_instance_id,
      base::BindOnce(view_it->second.cleanup_function, context_,
                     embedder_process_id, view_instance_id));
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

GuestViewBase* GuestViewManager::CreateGuestInternal(
    content::WebContents* owner_web_contents,
    const std::string& view_type) {
  if (guest_view_registry_.empty())
    RegisterGuestViewTypes();

  auto it = guest_view_registry_.find(view_type);
  if (it == guest_view_registry_.end()) {
    NOTREACHED();
    return nullptr;
  }

  return it->second.create_function.Run(owner_web_contents);
}

void GuestViewManager::RegisterGuestViewTypes() {
  delegate_->RegisterAdditionalGuestViewTypes();
}

void GuestViewManager::RegisterViewDestructionCallback(
    int embedder_process_id,
    int view_instance_id,
    base::OnceClosure callback) {
  // When an embedder is registered for the first time, create an observer to
  // watch for its destruction.
  if (!embedders_observed_.count(embedder_process_id)) {
    RenderProcessHost* rph = RenderProcessHost::FromID(embedder_process_id);
    // The RenderProcessHost may already be gone.
    if (!rph) {
      std::move(callback).Run();
      return;
    }

    embedders_observed_.insert(embedder_process_id);
    // EmbedderRenderProcessHostObserver owns itself.
    new EmbedderRenderProcessHostObserver(weak_ptr_factory_.GetWeakPtr(), rph);
  }

  view_destruction_callback_map_[embedder_process_id][view_instance_id]
      .push_back(std::move(callback));
}

bool GuestViewManager::IsGuestAvailableToContext(GuestViewBase* guest) {
  return delegate_->IsGuestAvailableToContext(guest);
}

void GuestViewManager::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::DictionaryValue> args,
    GuestViewBase* guest,
    int instance_id) {
  // TODO(fsamuel): GuestViewManager should probably do something more useful
  // here like log an error if the event could not be dispatched.
  delegate_->DispatchEvent(event_name, std::move(args), guest, instance_id);
}

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int guest_instance_id) {
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return nullptr;
  return it->second;
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

// static
bool GuestViewManager::GetFullPageGuestHelper(
    content::WebContents** result,
    content::WebContents* guest_web_contents) {
  auto* guest_view = GuestViewBase::FromWebContents(guest_web_contents);
  if (guest_view && guest_view->is_full_page_plugin()) {
    *result = guest_web_contents;
    return true;
  }
  return false;
}

bool GuestViewManager::CanEmbedderAccessInstanceID(
    int embedder_render_process_id,
    int guest_instance_id) {
  // TODO(780728): Remove crash key once the cause of the kill is known.
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
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;

  auto* guest_view = GuestViewBase::FromWebContents(it->second);
  if (!guest_view) {
    bad_access_key.Set("No guest");
    return false;
  }

  // MimeHandlerViewGuests (PDF) may be embedded in a cross-process frame.
  // Other than MimeHandlerViewGuest, all other guest types are only permitted
  // to run in the main frame or its local subframes.
  const int allowed_embedder_render_process_id =
      guest_view->CanBeEmbeddedInsideCrossProcessFrames()
          ? guest_view->GetOwnerSiteInstance()->GetProcess()->GetID()
          : guest_view->owner_web_contents()
                ->GetMainFrame()
                ->GetProcess()
                ->GetID();

  if (embedder_render_process_id != allowed_embedder_render_process_id) {
    bad_access_key.Set("Bad embedder process");
    return false;
  }

  return true;
}

GuestViewManager::ElementInstanceKey::ElementInstanceKey()
    : embedder_process_id(content::ChildProcessHost::kInvalidUniqueID),
      element_instance_id(content::ChildProcessHost::kInvalidUniqueID) {
}

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

bool GuestViewManager::ElementInstanceKey::operator==(
    const GuestViewManager::ElementInstanceKey& other) const {
  return (embedder_process_id == other.embedder_process_id) &&
         (element_instance_id == other.element_instance_id);
}

GuestViewManager::GuestViewData::GuestViewData(
    const GuestViewCreateFunction& create_function,
    const GuestViewCleanUpFunction& cleanup_function)
    : create_function(create_function), cleanup_function(cleanup_function) {}

GuestViewManager::GuestViewData::GuestViewData(const GuestViewData& other) =
    default;

GuestViewManager::GuestViewData::~GuestViewData() {}

}  // namespace guest_view
