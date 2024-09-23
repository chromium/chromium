// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PER_PROFILE_WEBUI_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PER_PROFILE_WEBUI_TRACKER_H_

#include <memory>
#include <string>

#include "base/observer_list_types.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

// PerProfileWebUITracker tracks WebUIs associated with a profile. It provides
// an API to query if a profile has any WebUI instances of a given URL. This
// class considers only the existence of a WebUI's WebContents, ignoring its
// status.
class PerProfileWebUITracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a tracked WebContents is destroyed.
    virtual void OnWebContentsDestroyed(content::WebContents* web_contents) = 0;
    // Called when a tracked WebContents' primary page changed.
    virtual void OnWebContentsPrimaryPageChanged(
        content::WebContents* web_contents) = 0;
  };

  static std::unique_ptr<PerProfileWebUITracker> Create();

  virtual ~PerProfileWebUITracker() = default;

  // Starts tracking `web_contents`.
  // This class automatically handles the destruction of WebContents, so manual
  // handling is not needed.
  virtual void AddWebContents(content::WebContents* web_contents) = 0;

  // Returns true if a WebUI with the specified URL exists within the profile.
  // This includes all WebContents, regardless of their visibility, hidden
  // state, crash state, or any errors.
  virtual bool ProfileHasWebUI(Profile* profile,
                               std::string webui_url) const = 0;

  // Adds an observer that will be notified of tracked WebContents destroy.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PER_PROFILE_WEBUI_TRACKER_H_
