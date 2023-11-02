// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/public/context_menu_download.h"

#include "components/download/public/common/download_url_parameters.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {

void CreateContextMenuDownload(content::WebContents* web_contents,
                               const content::ContextMenuParams& params,
                               const std::string& origin,
                               bool is_link) {
  const GURL& url = is_link ? params.link_url : params.src_url;
  const GURL& referring_url =
      params.frame_url.is_empty() ? params.page_url : params.frame_url;
  content::DownloadManager* dlm =
      web_contents->GetBrowserContext()->GetDownloadManager();
  std::unique_ptr<download::DownloadUrlParameters> dl_params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url,
          TRAFFIC_ANNOTATION_WITHOUT_PROTO("Download via context menu")));
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));

  if (is_link)
    dl_params->set_referrer_encoding(params.frame_charset);
  if (!is_link)
    dl_params->set_prefer_cache(true);
  dl_params->set_prompt(false);
  dl_params->set_request_origin(origin);
  dl_params->set_suggested_name(params.suggested_filename);
  dl_params->set_download_source(download::DownloadSource::CONTEXT_MENU);
  dlm->DownloadUrl(std::move(dl_params));
}

}  // namespace download
