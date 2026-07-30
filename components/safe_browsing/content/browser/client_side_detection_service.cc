// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include <algorithm>
#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_embedder.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

ClientSideDetectionService::ClientSideDetectionService(
    std::unique_ptr<Delegate> delegate_ptr,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : ClientSideDetectionServiceBase(std::move(delegate_ptr), opt_guide) {
  if (!delegate() || !prefs()) {
    return;
  }

  //  Do an initial check of the prefs.
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void ClientSideDetectionService::OnModelUpdated() {
  if (IsEnabled()) {
    SendModelToRenderers();
  }
}

void ClientSideDetectionService::SendModelToRenderers() {
  // We will not send models to the renderer process if the feature is disabled.
  // This is because the feature can be disabled via Finch in a scenario where a
  // bad model is uploaded to the server.
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    // TODO(crbug.com/502615476): Once `ClientSideDetectionService::Delegate` is
    // removed, this conditional can be simplified to
    // `if (it.GetCurrentValue()->GetBrowserContext() == browser_context_)`.
    if (delegate() && delegate()->ShouldSendModelToBrowserContext(
                          it.GetCurrentValue()->GetBrowserContext())) {
      auto* rph = it.GetCurrentValue();
      if (rph->IsReady()) {
        SetPhishingModel(rph, /*new_renderer_process_host=*/false);
      } else {
        if (rph->IsInitializedAndNotDead() &&
            !observed_render_process_hosts_.IsObservingSource(rph)) {
          observed_render_process_hosts_.AddObservation(rph);
        }
      }
    }
  }
  if (client_side_phishing_model_) {
    trigger_model_version_ =
        client_side_phishing_model_->GetTriggerModelVersion();
  }
}

void ClientSideDetectionService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void ClientSideDetectionService::OnRenderProcessHostCreated(
    content::RenderProcessHost* rph) {
  // TODO(crbug.com/502615476): Once `ClientSideDetectionService::Delegate` is
  // removed, this conditional can be simplified to
  // `if (rph->GetBrowserContext() == browser_context_)`.
  if (delegate() &&
      delegate()->ShouldSendModelToBrowserContext(rph->GetBrowserContext())) {
    // The |rph| is ready, so the model can immediately be send.
    if (rph->IsReady()) {
      SetPhishingModel(rph, /*new_renderer_process_host=*/true);
    } else if (!observed_render_process_hosts_.IsObservingSource(rph)) {
      // Postpone sending the model until the |rph| is ready.
      observed_render_process_hosts_.AddObservation(rph);
    }
  }
}

void ClientSideDetectionService::RenderProcessHostDestroyed(
    content::RenderProcessHost* rph) {
  if (observed_render_process_hosts_.IsObservingSource(rph)) {
    observed_render_process_hosts_.RemoveObservation(rph);
  }
}

void ClientSideDetectionService::RenderProcessReady(
    content::RenderProcessHost* rph) {
  SetPhishingModel(rph, /*new_renderer_process_host=*/true);
  if (observed_render_process_hosts_.IsObservingSource(rph)) {
    observed_render_process_hosts_.RemoveObservation(rph);
  }
}

void ClientSideDetectionService::SetPhishingModel(
    content::RenderProcessHost* rph,
    bool new_renderer_process_host) {
  // We want to check if the trigger model has been sent. If we have received a
  // callback after sending the trigger models before and the models are now
  // unavailable, that means the OptimizationGuide server sent us a null model
  // to signal that a bad model is in disk.
  if (!IsModelAvailable() && !sent_trigger_models_) {
    return;
  }
  if (!rph->GetChannel()) {
    return;
  }

  mojo::AssociatedRemote<mojom::PhishingModelSetter> model_setter;
  rph->GetChannel()->GetRemoteAssociatedInterface(&model_setter);
  if (!IsModelAvailable() && sent_trigger_models_) {
    model_setter->ClearScorer();
    return;
  }

  if (prefs() && IsEnhancedProtectionEnabled(*prefs())) {
    // The check for image embedding model is important because the
    // OptimizationGuide server can send a null image embedding model to
    // signal there is a bad model in disk. If the image embedding model
    // isn't available because of this, the scorer will be created without
    // the image embedder model, temporarily halting the image embedding
    // process on the renderer.
    if (IsModelMetadataImageEmbeddingVersionMatching() &&
        HasImageEmbeddingModel()) {
      base::UmaHistogramBoolean(
          "SBClientPhishing.ImageEmbeddingModelVersionMatch", true);
      if (!new_renderer_process_host &&
          trigger_model_version_ ==
              client_side_phishing_model_->GetTriggerModelVersion()) {
        // If the trigger model version remains the same in the same
        // renderer process host, we can just attach the complementary
        // image embedding model to the current scorer.
        model_setter->AttachImageEmbeddingModelAndDimensions(
            GetImageEmbeddingInputWidth(), GetImageEmbeddingInputHeight(),
            GetImageEmbeddingModel().Duplicate());
      } else {
        model_setter->SetImageEmbeddingAndPhishingTfLiteModel(
            GetClassificationInputWidth(), GetClassificationInputHeight(),
            GetVisualTfLiteModel().Duplicate(), GetImageEmbeddingInputWidth(),
            GetImageEmbeddingInputHeight(),
            GetImageEmbeddingModel().Duplicate());
      }
    } else {
      base::UmaHistogramBoolean(
          "SBClientPhishing.ImageEmbeddingModelVersionMatch", false);
      model_setter->SetPhishingTfLiteModel(GetClassificationInputWidth(),
                                           GetClassificationInputHeight(),
                                           GetVisualTfLiteModel().Duplicate());
    }
  } else {
    model_setter->SetPhishingTfLiteModel(GetClassificationInputWidth(),
                                         GetClassificationInputHeight(),
                                         GetVisualTfLiteModel().Duplicate());
  }
  sent_trigger_models_ = true;
}

base::WeakPtr<ClientSideDetectionService>
ClientSideDetectionService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ClientSideDetectionService::DidSendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    const std::string& access_token) {
  // The following is to log this ClientPhishingRequest on any open
  // chrome://safe-browsing pages. If no such page is open, the request is
  // dropped and the |request| object deleted.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebUIContentInfoSingleton::AddToClientPhishingRequestsSent,
          base::Unretained(WebUIContentInfoSingleton::GetInstance()),
          std::move(request), access_token));
}

void ClientSideDetectionService::DidReceiveClientPhishingResponse(
    const ClientPhishingResponse& response) {
  // The following is to log this ClientPhishingResponse on any open
  // chrome://safe-browsing pages. If no such page is open, the response is
  // dropped and the |response| object deleted.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebUIContentInfoSingleton::AddToClientPhishingResponsesReceived,
          base::Unretained(WebUIContentInfoSingleton::GetInstance()),
          std::make_unique<ClientPhishingResponse>(response)));
}

}  // namespace safe_browsing
