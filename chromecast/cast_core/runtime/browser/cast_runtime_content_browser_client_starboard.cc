// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client_starboard.h"

#include "base/memory/scoped_refptr.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/media/cdm/cast_cdm.h"
#include "chromecast/media/cdm/cast_cdm_factory.h"
#include "chromecast/media/cdm/cast_cdm_origin_provider.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "chromecast/service/cast_service.h"
#include "chromecast/starboard/media/cdm/starboard_decryptor_cast.h"
#include "content/public/browser/web_contents.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"

namespace chromecast {
namespace shell {

namespace {

// A CdmFactory that constructs StarboardDecryptorCast instances. These
// instances do not do actual decryption; decryption is done in the starboard
// library itself. Instead, StarboardDecryptorCast only acts as an intermediary
// between the Javascript Cast application and Starboard.
class StarboardCdmFactory : public chromecast::media::CastCdmFactory {
 public:
  StarboardCdmFactory(
      ::media::CreateFetcherCB create_provision_fetcher_cb,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const url::Origin& cdm_origin,
      chromecast::media::MediaResourceTracker* media_resource_tracker)
      : chromecast::media::CastCdmFactory(std::move(task_runner),
                                          cdm_origin,
                                          media_resource_tracker),
        create_provision_fetcher_cb_(std::move(create_provision_fetcher_cb)),
        media_resource_tracker_(media_resource_tracker) {}

  // Disallow copy and assign.
  StarboardCdmFactory(const StarboardCdmFactory&) = delete;
  StarboardCdmFactory& operator=(const StarboardCdmFactory&) = delete;

  ~StarboardCdmFactory() override = default;

  // CastCdmFactory implementation:
  scoped_refptr<chromecast::media::CastCdm> CreatePlatformBrowserCdm(
      const chromecast::media::CastKeySystem& cast_key_system,
      const url::Origin& cdm_origin,
      const ::media::CdmConfig& cdm_config) override {
    if (cast_key_system != media::CastKeySystem::KEY_SYSTEM_WIDEVINE) {
      LOG(ERROR) << "Widevine is the only DRM system supported via Starboard.";
      return nullptr;
    }

    return base::MakeRefCounted<media::StarboardDecryptorCast>(
        create_provision_fetcher_cb_, media_resource_tracker_);
  }

 private:
  ::media::CreateFetcherCB create_provision_fetcher_cb_;
  chromecast::media::MediaResourceTracker* media_resource_tracker_ = nullptr;
};

}  // namespace

CastRuntimeContentBrowserClientStarboard::
    CastRuntimeContentBrowserClientStarboard(
        CastFeatureListCreator* feature_list_creator)
    : CastRuntimeContentBrowserClient(feature_list_creator) {}

CastRuntimeContentBrowserClientStarboard::
    ~CastRuntimeContentBrowserClientStarboard() = default;

std::unique_ptr<::media::CdmFactory>
CastRuntimeContentBrowserClientStarboard::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  url::Origin cdm_origin;
  if (!CastCdmOriginProvider::GetCdmOrigin(frame_interfaces, &cdm_origin)) {
    LOG(ERROR) << "Failed to get CDM origin";
    return nullptr;
  }

  ::media::CreateFetcherCB provision_fetcher_cb = base::BindRepeating(
      [](::media::mojom::FrameInterfaceFactory* frame_interfaces)
          -> std::unique_ptr<::media::ProvisionFetcher> {
        mojo::PendingRemote<::media::mojom::ProvisionFetcher> provision_fetcher;
        frame_interfaces->CreateProvisionFetcher(
            provision_fetcher.InitWithNewPipeAndPassReceiver());
        return std::make_unique<::media::MojoProvisionFetcher>(
            std::move(provision_fetcher));
      },
      frame_interfaces);

  return std::make_unique<StarboardCdmFactory>(std::move(provision_fetcher_cb),
                                               GetMediaTaskRunner(), cdm_origin,
                                               media_resource_tracker());
}

void CastRuntimeContentBrowserClientStarboard::OnWebContentsCreated(
    content::WebContents* web_contents) {
  CastRuntimeContentBrowserClient::OnWebContentsCreated(web_contents);
  LOG(INFO) << "Creating a new StarboardWebContentsObserver";
  content_observer_.emplace(web_contents);
}

CastRuntimeContentBrowserClientStarboard::StarboardWebContentsObserver::
    StarboardWebContentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

CastRuntimeContentBrowserClientStarboard::StarboardWebContentsObserver::
    ~StarboardWebContentsObserver() = default;

void CastRuntimeContentBrowserClientStarboard::StarboardWebContentsObserver::
    LoadProgressChanged(double progress) {
  if (progress == 1.0) {
    LOG(INFO) << "Focusing web contents";
    web_contents()->Focus();
  }
}

}  // namespace shell
}  // namespace chromecast
