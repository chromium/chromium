// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBUI_CAST_RESOURCE_DATA_SOURCE_H_
#define CHROMECAST_BROWSER_WEBUI_CAST_RESOURCE_DATA_SOURCE_H_

#include <optional>
#include <string>

#include "chromecast/browser/webui/mojom/webui.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

// A minimal subset of content::WebUiDataSource logic has been pulled into
// CastResourceDataSource, in order to maintain parity with the previous design.
class CastResourceDataSource : public content::URLDataSource {
 public:
  CastResourceDataSource(const std::string& host, bool for_webui);
  ~CastResourceDataSource() override;

  mojo::PendingReceiver<mojom::Resources> BindNewPipeAndPassReceiver();
  void OverrideContentSecurityPolicyChildSrc(const std::string& data);
  void DisableDenyXFrameOptions();

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
  bool ShouldDenyXFrameOptions() override;

 private:
  const std::string host_;
  const bool for_webui_;
  mojo::Remote<mojom::Resources> remote_;

  std::optional<std::string> frame_src_;
  bool deny_xframe_options_ = true;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBUI_CAST_RESOURCE_DATA_SOURCE_H_
