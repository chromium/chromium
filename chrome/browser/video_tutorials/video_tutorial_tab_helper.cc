// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/video_tutorial_tab_helper.h"

#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

namespace video_tutorials {
namespace {

// Returns whether the URL of the navigation matches the video player URL.
bool IsVideoPlayerURL(GURL url) {
#if BUILDFLAG(IS_ANDROID)
  std::string url_string = url.possibly_invalid_spec();
  if (url_string.find(chrome::kChromeUIUntrustedVideoPlayerUrl) == 0)
    return true;
#endif
  return false;
}

}  // namespace

VideoTutorialTabHelper::VideoTutorialTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<VideoTutorialTabHelper>(*web_contents) {}

VideoTutorialTabHelper::~VideoTutorialTabHelper() = default;

void VideoTutorialTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  std::string url = handle->GetURL().possibly_invalid_spec();
  if (!IsVideoPlayerURL(handle->GetURL()))
    return;

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  handle->GetRenderFrameHost()->GetRemoteAssociatedInterfaces()->GetInterface(
      &client);
  client->AddAutoplayFlags(url::Origin::Create(handle->GetURL()),
                           blink::mojom::kAutoplayFlagUserException);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VideoTutorialTabHelper);

}  // namespace video_tutorials
