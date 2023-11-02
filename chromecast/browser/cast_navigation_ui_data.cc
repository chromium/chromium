// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_navigation_ui_data.h"

#include "chromecast/browser/cast_session_id_map.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace shell {
namespace {

const char kUserDataKey[] = "chromecast.shell.SessionIdUserData key";

class SessionIdUserData : public base::SupportsUserData::Data {
 public:
  explicit SessionIdUserData(const std::string& session_id)
      : session_id_(session_id) {}

  const std::string& session_id() const { return session_id_; }

 private:
  std::string session_id_;
};

}  // namespace

// static
void CastNavigationUIData::SetAppPropertiesForWebContents(
    content::WebContents* web_contents,
    const std::string& session_id,
    bool is_audio_app) {
  DCHECK(web_contents);
  web_contents->SetUserData(kUserDataKey,
                            std::make_unique<SessionIdUserData>(session_id));
  CastSessionIdMap::GetInstance()->SetAppProperties(session_id, is_audio_app,
                                                    web_contents);
}

// static
std::string CastNavigationUIData::GetSessionIdForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  SessionIdUserData* data =
      static_cast<SessionIdUserData*>(web_contents->GetUserData(kUserDataKey));
  return data ? data->session_id() : "";
}

CastNavigationUIData::CastNavigationUIData(const std::string& session_id)
    : session_id_(session_id) {}

std::unique_ptr<content::NavigationUIData> CastNavigationUIData::Clone() {
  return std::make_unique<CastNavigationUIData>(session_id_);
}

}  // namespace shell
}  // namespace chromecast
