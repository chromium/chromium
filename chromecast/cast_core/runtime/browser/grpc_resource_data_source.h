// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_RESOURCE_DATA_SOURCE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_RESOURCE_DATA_SOURCE_H_

#include <optional>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/url_data_source.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"

namespace chromecast {

// This class is responsible for making data requests for resources required by
// Cast WebUI. This implementation uses gRPC for requesting resources from
// CastCore.
class GrpcResourceDataSource : public content::URLDataSource {
 public:
  GrpcResourceDataSource(
      const std::string host,
      bool for_webui,
      cast::v2::CoreApplicationServiceStub* core_app_service_stub);
  ~GrpcResourceDataSource() override;

  void OverrideContentSecurityPolicyChildSrc(const std::string& data);
  void DisableDenyXFrameOptions();

 private:
  friend class GrpcResourceDataSourceTest;

  // content::URLDataSource implementation.
  std::string GetSource() override;

  // Starts a gRPC request to fetch resources.
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;

  // Determines whether the Url request is allowed.
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

  // Checks origin of the data request url.
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;

  bool ShouldDenyXFrameOptions() override;

  // Helper methods.
  void OnWebUiResourceReceived(
      content::URLDataSource::GotDataCallback callback,
      cast::utils::GrpcStatusOr<cast::v2::GetWebUIResourceResponse>
          response_or);
  void ReadResourceFile(std::string_view path,
                        content::URLDataSource::GotDataCallback callback);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const std::string host_;

  // This is set to true when GrpcResourceDataSource is initialized by
  // GrpcWebUIController and false otherwise. In practice, for all the
  // chrome://home/* urls this is set to true and false for chrome-resource://*
  const bool for_webui_;
  cast::v2::CoreApplicationServiceStub* const core_app_service_stub_;

  std::optional<std::string> frame_src_;
  bool deny_xframe_options_ = true;

  base::WeakPtrFactory<GrpcResourceDataSource> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_RESOURCE_DATA_SOURCE_H_
