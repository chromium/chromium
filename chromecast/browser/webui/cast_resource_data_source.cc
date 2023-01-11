// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_resource_data_source.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chromecast/base/cast_constants.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace chromecast {

namespace {

void GotData(base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)> cb,
             scoped_refptr<base::RefCountedMemory> memory) {
  std::move(cb).Run(std::move(memory));
}

}  // namespace

CastResourceDataSource::CastResourceDataSource(const std::string& host,
                                               bool for_webui)
    : host_(host), for_webui_(for_webui) {}

CastResourceDataSource::~CastResourceDataSource() = default;

std::string CastResourceDataSource::GetSource() {
  return host_;
}

void CastResourceDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string path = content::URLDataSource::URLToRequestPath(url);
  remote_->RequestResourceBytes(path,
                                base::BindOnce(&GotData, std::move(callback)));
}

std::string CastResourceDataSource::GetMimeType(const GURL& url) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);

  if (!for_webui_) {
    std::string mime_type;
    base::FilePath::StringType file_ext =
        base::FilePath().AppendASCII(path).Extension();
    // net::GetMimeTypeFromFile() will crash at base::nix::GetFileMimeType()
    // because IO is not allowed.
    if (!file_ext.empty())
      net::GetWellKnownMimeTypeFromExtension(file_ext.substr(1), &mime_type);
    return mime_type;
  }

  // Remove the query string for to determine the mime type.
  std::string file_path = path.substr(0, path.find_first_of('?'));

  if (base::EndsWith(file_path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";

  if (base::EndsWith(file_path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  if (base::EndsWith(file_path, ".json", base::CompareCase::INSENSITIVE_ASCII))
    return "application/json";

  if (base::EndsWith(file_path, ".pdf", base::CompareCase::INSENSITIVE_ASCII))
    return "application/pdf";

  if (base::EndsWith(file_path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";

  if (base::EndsWith(file_path, ".jpg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/jpeg";

  if (base::EndsWith(file_path, ".png", base::CompareCase::INSENSITIVE_ASCII))
    return "image/png";

  return "text/html";
}

bool CastResourceDataSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  if (url.SchemeIs(kChromeResourceScheme))
    return true;
  return URLDataSource::ShouldServiceRequest(url, browser_context,
                                             render_process_id);
}

std::string CastResourceDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  // For now we give access for all "chrome://*" origins.
  std::string allowed_origin_prefix = "chrome://";
  if (!base::StartsWith(origin, allowed_origin_prefix,
                        base::CompareCase::SENSITIVE)) {
    return "";
  }
  return origin;
}

mojo::PendingReceiver<mojom::Resources>
CastResourceDataSource::BindNewPipeAndPassReceiver() {
  return remote_.BindNewPipeAndPassReceiver();
}

void CastResourceDataSource::OverrideContentSecurityPolicyChildSrc(
    const std::string& data) {
  frame_src_ = data;
}

void CastResourceDataSource::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

std::string CastResourceDataSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  if (directive == network::mojom::CSPDirectiveName::ChildSrc && frame_src_) {
    return *frame_src_;
  }
  return URLDataSource::GetContentSecurityPolicy(directive);
}

bool CastResourceDataSource::ShouldDenyXFrameOptions() {
  return deny_xframe_options_;
}

}  // namespace chromecast
