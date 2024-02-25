// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_caps_impl.h"

#include "base/logging.h"
#include "chromecast/public/media/decoder_config.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace media {

mojom::CodecProfileLevelPtr ConvertCodecProfileLevelToMojo(
    const CodecProfileLevel& codec_profile_level) {
  mojom::CodecProfileLevelPtr result = mojom::CodecProfileLevel::New();
  result->codec = codec_profile_level.codec;
  result->profile = codec_profile_level.profile;
  result->level = codec_profile_level.level;
  return result;
}

MediaCapsImpl::MediaCapsImpl() = default;
MediaCapsImpl::~MediaCapsImpl() = default;

void MediaCapsImpl::AddReceiver(
    mojo::PendingReceiver<mojom::MediaCaps> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojo::PendingRemote<mojom::MediaCaps> MediaCapsImpl::GetPendingRemote() {
  mojo::PendingRemote<mojom::MediaCaps> pending_remote;
  AddReceiver(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void MediaCapsImpl::AddSupportedCodecProfileLevel(
    const CodecProfileLevel& codec_profile_level) {
  codec_profile_levels_.push_back(codec_profile_level);
  for (auto& observer : observers_) {
    mojom::CodecProfileLevelPtr mojo_codec_profile_level(
        ConvertCodecProfileLevelToMojo(codec_profile_level));
    observer->AddSupportedCodecProfileLevel(
        std::move(mojo_codec_profile_level));
  }
}

void MediaCapsImpl::AddObserver(
    mojo::PendingRemote<mojom::MediaCapsObserver> observer_remote) {
  mojo::Remote<mojom::MediaCapsObserver> observer(std::move(observer_remote));
  DVLOG(1) << __func__ << ": Sending " << codec_profile_levels_.size()
           << " supported codec profile levels to observer.";
  for (const auto& codec_profile_level : codec_profile_levels_) {
    observer->AddSupportedCodecProfileLevel(
        ConvertCodecProfileLevelToMojo(codec_profile_level));
  }
  observers_.Add(std::move(observer));
}

}  // namespace media
}  // namespace chromecast
