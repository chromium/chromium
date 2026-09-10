// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// InitialWebUIProfileService is responsible for pre-creating and navigating the
// initial WebUI early during Profile initialization.
class InitialWebUIProfileService : public KeyedService {
 public:
  explicit InitialWebUIProfileService(Profile* profile);
  InitialWebUIProfileService(const InitialWebUIProfileService&) = delete;
  InitialWebUIProfileService& operator=(const InitialWebUIProfileService&) =
      delete;
  ~InitialWebUIProfileService() override;

  // Returns the pre-created WebContents, passing ownership to the caller.
  // Returns nullptr if it has already been taken or prewarming is disabled.
  std::unique_ptr<content::WebContents> TakeToolbarContents();

  // KeyedService:
  void Shutdown() override;

  bool has_toolbar_contents_for_testing() const {
    return toolbar_web_contents_ != nullptr;
  }

 private:
  void PrewarmWebUI();

  const raw_ptr<Profile> profile_;
  std::unique_ptr<content::WebContents> toolbar_web_contents_;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_
