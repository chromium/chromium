// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/embedder_application.h"

#include <vector>

#include "base/supports_user_data.h"
#include "components/cast_receiver/browser/public/streaming_config_manager.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"

namespace cast_receiver {
namespace {

// Key in the WebContents's UserData where the singleton instance of
// TrivialStreamingConfigManager should be stored.
//
// TODO(crbug.com/1382897): Use the same key as the rest of the cast_receiver
// component's UserData.
const char kTrivialStreamingConfigManagerUserDataKey[] =
    "components/cast_receiver/browser/embedder_application:ConfigManager";

class TrivialStreamingConfigManager : public StreamingConfigManager,
                                      public base::SupportsUserData::Data {
 public:
  TrivialStreamingConfigManager() {
    std::vector<media::VideoCodec> video_codecs{media::VideoCodec::kVP8};
    std::vector<media::AudioCodec> audio_codecs{media::AudioCodec::kOpus};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    video_codecs.push_back(media::VideoCodec::kH264);
    audio_codecs.push_back(media::AudioCodec::kAAC);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

    OnStreamingConfigSet(cast_streaming::ReceiverConfig(
        std::move(video_codecs), std::move(audio_codecs)));
  }

  ~TrivialStreamingConfigManager() override = default;
};

}  // namespace

EmbedderApplication::~EmbedderApplication() = default;

StreamingConfigManager* EmbedderApplication::GetStreamingConfigManager() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  auto* instance = static_cast<TrivialStreamingConfigManager*>(
      web_contents->GetUserData(&kTrivialStreamingConfigManagerUserDataKey));
  if (instance) {
    return instance;
  }

  std::unique_ptr<TrivialStreamingConfigManager> config_manager =
      std::make_unique<TrivialStreamingConfigManager>();
  auto* ptr = config_manager.get();
  web_contents->SetUserData(&kTrivialStreamingConfigManagerUserDataKey,
                            std::move(config_manager));
  return ptr;
}

std::unique_ptr<content::WebUIControllerFactory>
EmbedderApplication::CreateWebUIControllerFactory(
    std::vector<std::string> hosts) {
  return nullptr;
}

void EmbedderApplication::NavigateToPage(const GURL& gurl) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);
  web_contents->GetController().LoadURL(gurl, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED, "");
}

std::ostream& operator<<(std::ostream& os,
                         EmbedderApplication::ApplicationStopReason reason) {
  switch (reason) {
    case EmbedderApplication::ApplicationStopReason::kUndefined:
      return os << "Undefined";
    case EmbedderApplication::ApplicationStopReason::kApplicationRequest:
      return os << "Application Request";
    case EmbedderApplication::ApplicationStopReason::kIdleTimeout:
      return os << "Idle Timeout";
    case EmbedderApplication::ApplicationStopReason::kUserRequest:
      return os << "Use Request";
    case EmbedderApplication::ApplicationStopReason::kHttpError:
      return os << "HTTP Error";
    case EmbedderApplication::ApplicationStopReason::kRuntimeError:
      return os << "Runtime Error";
  }
}

}  // namespace cast_receiver
