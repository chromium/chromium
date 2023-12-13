// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay_driver.h"
#include "chrome/browser/ui/webui/record_replay/record_replay_manager_handler.h"
#include "components/record_replay/services/auth_token/public/cpp/auth_token_service_factory.h"
#include <string>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

RecordReplayManagerHandler::RecordReplayManagerHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::RecordReplayManagerHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {
}

RecordReplayManagerHandler::~RecordReplayManagerHandler() = default;

void RecordReplayManagerHandler::SetManager(
    mojo::PendingRemote<mojom::RecordReplayManager> manager) {
  manager_.Bind(std::move(manager));
}

void RecordReplayManagerHandler::HandleSignInButtonClicked() {
  fprintf(stderr, "RecordReplay [RUN-2866] ManagerHandler(%p)::HandleSignInButtonClicked()\n", this);
  manager_->HandleSignInButtonClicked();
  // auth_token::RecordReplayAuthTokenServiceFactory::GetForBrowserContext(profile_)->SetToken(api_key);
}

void RecordReplayManagerHandler::GetEnv(const std::string& key, GetEnvCallback callback) {
  std::move(callback).Run(absl::optional<std::string>());
}
void RecordReplayManagerHandler::GetBuildId(GetBuildIdCallback callback) {
  std::move(callback).Run(RECORD_REPLAY_BUILD_ID);
}
void RecordReplayManagerHandler::GetReplayUserToken(GetReplayUserTokenCallback callback) {
  std::move(callback).Run(record_replay_user_token_);
}
void RecordReplayManagerHandler::SetReplayUserToken(const absl::optional<std::string>& token) {
  record_replay_user_token_ = token;
}
void RecordReplayManagerHandler::GetReplayRefreshToken(GetReplayRefreshTokenCallback callback) {
  std::move(callback).Run(record_replay_refresh_token_);
}
void RecordReplayManagerHandler::SetReplayRefreshToken(const absl::optional<std::string>& token) {
  record_replay_refresh_token_ = token;
}
void RecordReplayManagerHandler::ShowAuthenticationError(const std::string& message) {
  fprintf(stderr, "RecordReplay [RUN-2866] ManagerHandler(%p)::ShowAuthenticationError(%s)\n", this,
         message.c_str());
}

#if BUILDFLAG(IS_MAC)
static void OpenExternalBrowserMac(const std::string& url_str) {
  CFURLRef url = CFURLCreateWithBytes (
      NULL,                        // allocator
      (UInt8*)url_str.c_str(),     // URLBytes
      url_str.length(),            // length
      kCFStringEncodingASCII,      // encoding
      NULL                         // baseURL
    );
  LSOpenCFURLRef(url,0);
  CFRelease(url);
}
#endif

#if BUILDFLAG(IS_LINUX)
static void OpenExternalBrowserLinux(const std::string& url_str) {
  std::string cmd = "xdg-open '" + url_str + "'";
  int result = system(cmd.c_str());
  if (result != 0) {
    fprintf(stderr, "RecordReplayManagerHandler::OpenExternalBrowserLinux() failed with %d\n",
           result);
  }
}
#endif

#if BUILDFLAG(IS_WIN)
static void OpenExternalBrowserWindows(const std::string& url_str) {
  fprintf(stderr, "RecordReplayManagerHandler::OpenExternalBrowserWindows() NOT IMPLEMENTED\n");
}
#endif

void RecordReplayManagerHandler::OpenExternalBrowser(const std::string& url) {
#if BUILDFLAG(IS_MAC)
  OpenExternalBrowserMac(url);
#elif BUILDFLAG(IS_LINUX)
  OpenExternalBrowserLinux(url);
#elif BUILDFLAG(IS_WIN)
  OpenExternalBrowserWindows(url);
#endif
}
