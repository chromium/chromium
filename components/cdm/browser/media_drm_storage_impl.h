// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_
#define COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class RenderFrameHost;
}

namespace cdm {

namespace prefs {

extern const char kMediaDrmStorage[];

}  // namespace prefs

// Implements media::mojom::MediaDrmStorage using PrefService.
// This file is located under components/ so that it can be shared by multiple
// content embedders (e.g. chrome and chromecast).
class MediaDrmStorageImpl final
    : public content::DocumentService<media::mojom::MediaDrmStorage> {
 public:
  // When using per-origin provisioning, this is the ID for the origin.
  // If not specified, the device specific origin ID is to be used.
  using MediaDrmOriginId = std::optional<base::UnguessableToken>;

  // |success| is true if an origin ID was obtained and |origin_id| is
  // specified, false otherwise.
  using OriginIdObtainedCB =
      base::OnceCallback<void(bool success, const MediaDrmOriginId& origin_id)>;
  using GetOriginIdCB = base::RepeatingCallback<void(OriginIdObtainedCB)>;

  // |callback| returns true if an empty origin ID is allowed, false if not.
  using AllowEmptyOriginIdCB =
      base::RepeatingCallback<void(base::OnceCallback<void(bool)> callback)>;

  // |callback| returns true if Licenses matching the filter are cleared, false
  // if not.
  using ClearMatchingLicensesFilterCB =
      base::RepeatingCallback<bool(const GURL&)>;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Get a list of origins that have persistent storage on the device.
  static std::set<GURL> GetAllOrigins(const PrefService* pref_service);

  // Get a list of all origins that have been modified after |start|
  // and before |end|.
  static std::vector<GURL> GetOriginsModifiedBetween(
      const PrefService* pref_service,
      base::Time start,
      base::Time end);

#if BUILDFLAG(IS_ANDROID)
  // Clear media licenses and related data if:
  // 1. Creation time falls in [delete_begin, delete_end], and
  // 2. |filter| returns true for the origin. |filter| is passed in to allow
  // licenses under specific origins to be cleared. Empty |filter| means remove
  // licenses for all origins.
  //
  // Media license session data will be removed from persist storage. Removing
  // the actual license file needs ack response from license server, so it's
  // hard for Chromium to do that. Since it's difficult to get the real id for
  // the license without the session data, we can treat the licenses as cleared.
  //
  // If all the licenses under the origin are cleared, the origin will be
  // unprovisioned, a.k.a the cert will be removed.
  // TODO(yucliu): Add unit test.
  static void ClearMatchingLicenses(PrefService* pref_service,
                                    base::Time start,
                                    base::Time end,
                                    const ClearMatchingLicensesFilterCB& filter,
                                    base::OnceClosure complete_cb);
#endif

  // |get_origin_id_cb| must be provided and is used to obtain an origin ID.
  // |allow_empty_origin_id_cb| is used to determine if an empty origin ID is
  // allowed or not. It is called if |get_origin_id_cb| is unable to return an
  // origin ID.
  MediaDrmStorageImpl(
      content::RenderFrameHost& render_frame_host,
      PrefService* pref_service,
      GetOriginIdCB get_origin_id_cb,
      AllowEmptyOriginIdCB allow_empty_origin_id_cb,
      mojo::PendingReceiver<media::mojom::MediaDrmStorage> receiver);

  // As above, but derives the PrefService from |render_frame_host|.
  // TODO(estade): make this the only constructor.
  MediaDrmStorageImpl(
      content::RenderFrameHost& render_frame_host,
      GetOriginIdCB get_origin_id_cb,
      AllowEmptyOriginIdCB allow_empty_origin_id_cb,
      mojo::PendingReceiver<media::mojom::MediaDrmStorage> receiver);

  // media::mojom::MediaDrmStorage implementation.
  void Initialize(InitializeCallback callback) final;
  void OnProvisioned(OnProvisionedCallback callback) final;
  void SavePersistentSession(const std::string& session_id,
                             media::mojom::SessionDataPtr session_data,
                             SavePersistentSessionCallback callback) final;
  void LoadPersistentSession(const std::string& session_id,
                             LoadPersistentSessionCallback callback) final;
  void RemovePersistentSession(const std::string& session_id,
                               RemovePersistentSessionCallback callback) final;

 private:
  // |this| can only be destructed as a DocumentService.
  ~MediaDrmStorageImpl() final;

  // Called when |get_origin_id_cb_| asynchronously returns a origin ID as part
  // of Initialize().
  void OnOriginIdObtained(bool success, const MediaDrmOriginId& origin_id);

  // Called after checking if an empty origin ID is allowed.
  void OnEmptyOriginIdAllowed(bool allowed);

  const raw_ptr<PrefService> pref_service_;
  GetOriginIdCB get_origin_id_cb_;
  AllowEmptyOriginIdCB allow_empty_origin_id_cb_;

  // ID for the current origin. Per EME spec on individualization,
  // implementation should not expose application-specific information.
  // If not specified, the device specific origin ID is to be used.
  MediaDrmOriginId origin_id_;

  // As Initialize() may be asynchronous, save the InitializeCallback when
  // necessary.
  InitializeCallback init_cb_;

  // Set when initialized.
  bool is_initialized_ = false;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaDrmStorageImpl> weak_factory_{this};
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_
