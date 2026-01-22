// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"

#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/guest_view/browser/guest_view_histogram_value.h"
#include "components/guest_view/browser/slim_web_view/grit/slim_web_view_strings.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace {

const char kStoragePartitionId[] = "partition";

void ParsePartitionParam(const base::Value::Dict& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  const std::string* partition_str =
      create_params.FindString(kStoragePartitionId);
  if (!partition_str) {
    return;
  }

  // Since the "persist:" prefix is in ASCII, base::StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (base::StartsWith(*partition_str,
                       "persist:", base::CompareCase::SENSITIVE)) {
    size_t index = partition_str->find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str->substr(index + 1);

    if (storage_partition_id->empty()) {
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = *partition_str;
    *persist_storage = false;
  }
}

}  // namespace

namespace guest_view {

// static
const guest_view::GuestViewHistogramValue SlimWebViewGuest::HistogramValue =
    guest_view::GuestViewHistogramValue::kSlimWebView;

// static
std::unique_ptr<GuestViewBase> SlimWebViewGuest::Create(
    content::RenderFrameHost* owner_render_frame_host) {
  return base::WrapUnique(new SlimWebViewGuest(owner_render_frame_host));
}

SlimWebViewGuest::SlimWebViewGuest(
    content::RenderFrameHost* owner_render_frame_host)
    : GuestView<SlimWebViewGuest>(owner_render_frame_host) {}

bool SlimWebViewGuest::GuestHandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  return false;
}

const char* SlimWebViewGuest::GetAPINamespace() const {
  NOTREACHED() << "SlimWebView doesn't use the extensions API";
}

int SlimWebViewGuest::GetTaskPrefix() const {
  return IDS_TASK_MANAGER_SLIM_WEB_VIEW_TAG_PREFIX;
}

void SlimWebViewGuest::MaybeRecreateGuestContents(
    content::RenderFrameHost* outer_contents_frame) {
  NOTREACHED() << "new window creation is not supported in SlimWebView";
}

void SlimWebViewGuest::CreateInnerPage(
    std::unique_ptr<GuestViewBase> owned_this,
    scoped_refptr<content::SiteInstance> site_instance,
    const base::Value::Dict& create_params,
    GuestPageCreatedCallback callback) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // TODO(crbug.com/460804848): Complete the implementation for MPArch.
    NOTIMPLEMENTED();
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  if (site_instance) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    DVLOG(2) << "Rejected new window creation";
    return;
  }
  std::string storage_partition_id;
  bool persist_storage = false;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  content::StoragePartitionConfig partition_config =
      content::StoragePartitionConfig::Create(
          browser_context(),
          owner_rfh()->GetSiteInstance()->GetSiteURL().GetHost(),
          storage_partition_id, !persist_storage);

  scoped_refptr<content::SiteInstance> guest_site_instance =
      content::SiteInstance::CreateForGuest(browser_context(),
                                            partition_config);
  content::WebContents::CreateParams stored_params(
      browser_context(), std::move(guest_site_instance));
  stored_params.guest_delegate = this;
  SetCreateParams(create_params, stored_params);
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(stored_params);
  std::move(callback).Run(std::move(owned_this), std::move(new_contents));
}

}  // namespace guest_view
