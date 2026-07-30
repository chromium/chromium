// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class is used to fetch the client-side
// model and send it to all renderers.  This class is also used to send a ping
// back to Google to verify if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to be made on the UI
// thread.  We also expect that the calling thread runs a message loop.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"
#include "components/safe_browsing/core/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "net/base/ip_address.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class ClientPhishingRequest;

// Content implementation of ClientSideDetectionServiceBase which pushes models
// to the renderers, responds to classification requests. This owns two
// ModelLoader objects.
class ClientSideDetectionService
    : public ClientSideDetectionServiceBase,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  // TODO(crbug.com/502615476): Drop this delegate class entirely. Since
  // `ClientSideDetectionService` lives in content/browser, it has access to
  // `content::BrowserContext`. We can pass `content::BrowserContext*` directly
  // to the constructor and store it as a member variable.
  //
  // Delegate which allows to provide embedder specific implementations.
  class Delegate : public ClientSideDetectionServiceBase::Delegate {
   public:
    ~Delegate() override = default;

    virtual bool ShouldSendModelToBrowserContext(
        content::BrowserContext* context) = 0;
  };

  // TODO(crbug.com/502615476): Once `ClientSideDetectionService::Delegate` is
  // removed, update this to accept `content::BrowserContext*` and
  // `std::unique_ptr<ClientSideDetectionServiceBase::Delegate>`
  ClientSideDetectionService(
      std::unique_ptr<Delegate> delegate_ptr,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionService(const ClientSideDetectionService&) = delete;
  ClientSideDetectionService& operator=(const ClientSideDetectionService&) =
      delete;

  ~ClientSideDetectionService() override;



  // Sends a model to each renderer.
  virtual void SendModelToRenderers();

  // Overrides the SharedURLLoaderFactory
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Sends a model to each renderer.
  void SetPhishingModel(content::RenderProcessHost* rph,
                        bool new_renderer_process_host);

  // Returns a WeakPtr for this service.
  base::WeakPtr<ClientSideDetectionService> GetWeakPtr();

 private:
  // ClientSideDetectionServiceBase implementation:
  void OnModelUpdated() override;
  void DidSendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> request,
      const std::string& access_token) override;
  void DidReceiveClientPhishingResponse(
      const ClientPhishingResponse& response) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* rph) override;

  //  content::RenderProcessHostObserver
  void RenderProcessHostDestroyed(content::RenderProcessHost* rph) override;
  void RenderProcessReady(content::RenderProcessHost* rph) override;

  // TODO(crbug.com/502615476): Once `ClientSideDetectionService::Delegate` is
  // removed, this function should be deleted and we should rely on the
  // protected `delegate()` in the Base class.
  Delegate* delegate() const {
    return static_cast<Delegate*>(ClientSideDetectionServiceBase::delegate());
  }

  // Whether the trigger models have been sent or not. This is used to determine
  // whether an empty model in the model class determines whether the models
  // haven't been sent or we should clear the models in the scorer because they
  // have been sent.
  bool sent_trigger_models_ = false;

  // The version of the trigger model that was last sent to the renderers.
  int trigger_model_version_ = 0;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observed_render_process_hosts_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to asynchronously call the callbacks for
  // SendClientReportPhishingRequest.
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_
