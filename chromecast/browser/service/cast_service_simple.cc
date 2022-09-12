// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_simple.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"

namespace chromecast {
namespace shell {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL();

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

}  // namespace

CastServiceSimple::CastServiceSimple(CastWebService* web_service)
    : web_service_(web_service) {
  DCHECK(web_service_);
}

CastServiceSimple::~CastServiceSimple() {
}

void CastServiceSimple::InitializeInternal() {
  startup_url_ = GetStartupURL();
}

void CastServiceSimple::FinalizeInternal() {
}

void CastServiceSimple::StartInternal() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return;
  }

  if (startup_url_.is_empty()) {
    return;
  }

  ::chromecast::mojom::CastWebViewParamsPtr params =
      ::chromecast::mojom::CastWebViewParams::New();
  params->enabled_for_dev = true;

  cast_web_view_ = web_service_->CreateWebViewInternal(std::move(params));
  cast_web_view_->cast_web_contents()->LoadUrl(startup_url_);
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);
}

void CastServiceSimple::StopInternal() {
  if (cast_web_view_) {
    cast_web_view_->cast_web_contents()->ClosePage();
  }
  cast_web_view_.reset();
}

}  // namespace shell
}  // namespace chromecast
