// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DOM_DISTILLER_VIEWER_SOURCE_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DOM_DISTILLER_VIEWER_SOURCE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

class DomDistillerServiceInterface;
class DomDistillerViewerSourceTest;

// Serves HTML and resources for viewing distilled articles.
class DomDistillerViewerSource : public content::URLDataSource {
 public:
  DomDistillerViewerSource(DomDistillerServiceInterface* dom_distiller_service,
                           const std::string& scheme);
  ~DomDistillerViewerSource() override;

  class RequestViewerHandle;

  // Overridden from content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

  DomDistillerViewerSource(const DomDistillerViewerSource&) = delete;
  DomDistillerViewerSource& operator=(const DomDistillerViewerSource&) = delete;

 private:
  friend class DomDistillerViewerSourceTest;

  // The scheme this URLDataSource is hosted under.
  std::string scheme_;

  // The service which contains all the functionality needed to interact with
  // the list of articles.
  DomDistillerServiceInterface* dom_distiller_service_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DOM_DISTILLER_VIEWER_SOURCE_H_
