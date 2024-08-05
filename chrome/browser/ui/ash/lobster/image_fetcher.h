// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOBSTER_IMAGE_FETCHER_H_
#define CHROME_BROWSER_UI_ASH_LOBSTER_IMAGE_FETCHER_H_

#include <string_view>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace manta {

namespace proto {
class Response;
}  // namespace proto

class SnapperProvider;
struct MantaStatus;
}  // namespace manta

class ImageFetcher {
 public:
  explicit ImageFetcher(manta::SnapperProvider* provider);
  ~ImageFetcher();

  void RequestPreviewCandidates(std::string_view query,
                                int num_candidates,
                                ash::RequestCandidatesCallback);

 private:
  void OnRequestPreviewCandidates(
      ash::RequestCandidatesCallback,
      std::unique_ptr<manta::proto::Response> response,
      manta::MantaStatus status);

  raw_ptr<manta::SnapperProvider> provider_;

  base::WeakPtrFactory<ImageFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_LOBSTER_IMAGE_FETCHER_H_
