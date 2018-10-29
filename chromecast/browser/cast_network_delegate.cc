// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_delegate.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_navigation_ui_data.h"
#include "chromecast/browser/cast_network_request_interceptor.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/child_process_host.h"
#include "net/base/net_errors.h"

namespace chromecast {
namespace shell {

std::unique_ptr<CastNetworkDelegate> CastNetworkDelegate::Create() {
  return std::make_unique<CastNetworkDelegate>(
      CastNetworkRequestInterceptor::Create());
}

CastNetworkDelegate::CastNetworkDelegate(
    std::unique_ptr<CastNetworkRequestInterceptor> network_request_interceptor)
    : network_request_interceptor_(std::move(network_request_interceptor)) {
  DCHECK(network_request_interceptor_);
  DETACH_FROM_THREAD(thread_checker_);
}

CastNetworkDelegate::~CastNetworkDelegate() {
}

void CastNetworkDelegate::Initialize() {
  network_request_interceptor_->Initialize();
}

bool CastNetworkDelegate::IsWhitelisted(const GURL& gurl,
                                        const std::string& session_id,
                                        int render_process_id,
                                        int render_frame_id,
                                        bool for_device_auth) const {
  return network_request_interceptor_->IsWhiteListed(
      gurl, session_id, render_process_id, render_frame_id, for_device_auth);
}

bool CastNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  if (base::CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableLocalFileAccesses)) {
    return true;
  }

  LOG(WARNING) << "Could not access file " << original_path.value()
               << ". All file accesses are forbidden.";
  return false;
}

int CastNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  if (!network_request_interceptor_->IsInitialized())
    return net::OK;

  // Get session id
  std::string session_id;
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  CastNavigationUIData* nav_data =
      request_info ? static_cast<CastNavigationUIData*>(
                         request_info->GetNavigationUIData())
                   : nullptr;
  if (nav_data) {
    session_id = nav_data->session_id();
  }

  // Get render process PID
  int render_process_id;
  int render_frame_id;
  if (!content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id)) {
    render_process_id = content::ChildProcessHost::kInvalidUniqueID;
    render_frame_id = content::ChildProcessHost::kInvalidUniqueID;
  }
  return network_request_interceptor_->OnBeforeURLRequest(
      request, session_id, render_process_id, render_frame_id,
      std::move(callback), new_url);
}

int CastNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  if (!network_request_interceptor_->IsInitialized())
    return net::OK;
  return network_request_interceptor_->OnBeforeStartTransaction(
      request, std::move(callback), headers);
}

void CastNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  if (network_request_interceptor_->IsInitialized())
    network_request_interceptor_->OnURLRequestDestroyed(request);
}

}  // namespace shell
}  // namespace chromecast
