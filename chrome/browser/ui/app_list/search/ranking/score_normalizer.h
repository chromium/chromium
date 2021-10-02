// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/ranking/persistent_proto.h"
#include "chrome/browser/ui/app_list/search/ranking/score_normalizer.pb.h"

namespace app_list {

// The score normalizer is a heuristic model that attempts to map a incoming
// stream of numbers drawn from a fixed distribution into a uniform
// distribution.
//
// TODO(crbug.com/1199206): Add more description once the algorithm is
// finalized.
class ScoreNormalizer {
 public:
  // All user-settable parameters of the score normalizer. The struct has some
  // reasonable defaults, but this should be customized per use-case.
  struct Params {
    // A version for this set of parameters. This is saved alongside on-disk
    // state for the ScoreNormalizer and, if it doesn't match on reading, the
    // on-disk state is cleared.
    int32_t version = 1;

    // How long to wait until writing any updates to disk.
    base::TimeDelta write_delay = base::Seconds(30);

    // TODO(crbug.com/1199206): Add model parameters.
  };

  ScoreNormalizer(const base::FilePath& filepath, const Params& params);
  ~ScoreNormalizer();

  ScoreNormalizer(const ScoreNormalizer&) = delete;
  ScoreNormalizer& operator=(const ScoreNormalizer&) = delete;

  // TODO(crbug.com/1199206): Add API methods.

 private:
  friend class ScoreNormalizerTest;

  void OnProtoRead(ReadStatus status);

  PersistentProto<ScoreNormalizerProto> proto_;
  Params params_;

  base::WeakPtrFactory<ScoreNormalizer> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZER_H_
