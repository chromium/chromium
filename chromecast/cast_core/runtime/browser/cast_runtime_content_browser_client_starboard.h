// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_

#include <memory>
#include <optional>

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {
namespace shell {

// A client that creates Starboard-based CDM factories. These factories produce
// CDMs that allow decryption to be done in Starboard.
//
// Additionally, when WebContents have been fully loaded, a
// StarboardWebContentsObserver owned by this class will focus the contents.
// This is necessary for inputs to be received, e.g. by a remote control for the
// TV.
//
// Aside from this, this client is identical to CastRuntimeBrowserClient.
class CastRuntimeContentBrowserClientStarboard
    : public CastRuntimeContentBrowserClient {
 public:
  explicit CastRuntimeContentBrowserClientStarboard(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClientStarboard() override;

  // CastRuntimeContentBrowserClient implementation:
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;

  void OnWebContentsCreated(content::WebContents* web_contents) override;

 private:
  // The starboard cast web runtime requires that the web contents be focused
  // after the page has loaded. Otherwise, user inputs will not reach the
  // Javascript.
  //
  // This class simply waits for a page to load, then focuses the contents.
  class StarboardWebContentsObserver : public content::WebContentsObserver {
   public:
    StarboardWebContentsObserver(content::WebContents* web_contents);

    ~StarboardWebContentsObserver() override;

    // content::WebContentsObserver implementation:
    void LoadProgressChanged(double progress) override;
  };

  std::optional<StarboardWebContentsObserver> content_observer_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_
