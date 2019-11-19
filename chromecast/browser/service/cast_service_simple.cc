// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_simple.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
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
    return GURL("http://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

}  // namespace

CastServiceSimple::CastServiceSimple(content::BrowserContext* browser_context,
                                     PrefService* pref_service,
                                     CastWindowManager* window_manager)
    : CastService(browser_context, pref_service),
      web_view_factory_(std::make_unique<CastWebViewFactory>(browser_context)),
      web_service_(std::make_unique<CastWebService>(browser_context,
                                                    web_view_factory_.get(),
                                                    window_manager)) {
  shell::CastBrowserProcess::GetInstance()->SetWebViewFactory(
      web_view_factory_.get());
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

  CastWebView::CreateParams params;
  params.delegate = weak_factory_.GetWeakPtr();
  params.web_contents_params.delegate = weak_factory_.GetWeakPtr();
  params.web_contents_params.enabled_for_dev = true;
  params.window_params.delegate = weak_factory_.GetWeakPtr();
  cast_web_view_ =
      web_service_->CreateWebView(params, nullptr, /* site_instance */
                                  GURL() /* initial_url */);
  cast_web_view_->LoadUrl(startup_url_);
  cast_web_view_->GrantScreenAccess();
  cast_web_view_->InitializeWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);
}

void CastServiceSimple::StopInternal() {
  if (cast_web_view_) {
    cast_web_view_->ClosePage();
  }
  cast_web_view_.reset();
}

void CastServiceSimple::OnWindowDestroyed() {}

bool CastServiceSimple::CanHandleGesture(GestureType gesture_type) {
  return false;
}

bool CastServiceSimple::ConsumeGesture(GestureType gesture_type) {
  return false;
}

void CastServiceSimple::OnVisibilityChange(VisibilityType visibility_type) {}

std::string CastServiceSimple::GetId() {
  return "";
}

}  // namespace shell
}  // namespace chromecast
