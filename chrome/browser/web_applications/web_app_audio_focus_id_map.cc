// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"

#include "base/unguessable_token.h"

namespace web_app {

WebAppAudioFocusIdMap::WebAppAudioFocusIdMap() = default;

WebAppAudioFocusIdMap::~WebAppAudioFocusIdMap() = default;

const base::UnguessableToken& WebAppAudioFocusIdMap::CreateOrGetIdForApp(
    const webapps::AppId& app_id) {
  auto it = ids_.find(app_id);

  if (it == ids_.end())
    it = ids_.emplace(app_id, base::UnguessableToken::Create()).first;

  return it->second;
}

}  // namespace web_app
