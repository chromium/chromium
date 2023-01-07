// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_LOAD_TYPE_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_LOAD_TYPE_H_

namespace chromecast {
namespace media {

enum LoadType {
  kLoadTypeURL,
  kLoadTypeMediaSource,
  kLoadTypeMediaStream,
  kLoadTypeCommunication,
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_LOAD_TYPE_H_
