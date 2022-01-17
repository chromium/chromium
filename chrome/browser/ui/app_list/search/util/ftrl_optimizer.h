// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.pb.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"

namespace app_list {

// Represents a single expert in the FTRL optimizer.
class FtrlExpert {
 public:
  FtrlExpert() {}
  virtual ~FtrlExpert() {}

  FtrlExpert(const FtrlExpert&) = delete;
  FtrlExpert& operator=(const FtrlExpert&) = delete;

  virtual std::vector<double> Score(const std::vector<std::string>& items) = 0;
  virtual void Train(const std::string& item) = 0;
};

// A class implementing the follow-the-regularized-leader optimization
// algorithm.
//
// ALGORITHM
// =========
//
// This is an ensemble model over experts E[i], keeping a weight for each W[i].
// The Score method takes a vector of items, and requests a score from each
// expert. The final score for an item is simply a weighted average:
//
// Score(item):
//   return sum(W[i] * E[i].Score(item))
//
// After the user clicks an item, the weights are then updated to reflect the
// accuracy of each expert, like so:
//
// Train(item):
//   for expert i:
//     if E[i] did not rank item in its last scores:
//       loss = 1
//     else
//       loss = (rank of item in E[i]'s last scores) / scores.size
//
//     w[i] = gamma/num_experts + (1-gamma) exp(-alpha * loss) * w[i]
//
//   normalize weights to sum to 1.
//
// PARAMETERS
// ==========
//
// - Alpha is a positive learning rate. Higher values mean weights change more
//   quickly.
//
// - Gamma is a regularization parameter in [0,1]. It controls the minimum
//   weight of an expert. 0 means no regularization, and 1 means all weights
//   will always be equal.
class FtrlOptimizer {
 public:
  // All user-settable parameters of the FTRL optimizer. The defaults should be
  // customized as-needed.
  struct Params {
    // How long to wait until writing any updates to disk.
    base::TimeDelta write_delay = base::Seconds(5);
    // TODO(crbug.com/1199206): These need tweaking.
    double alpha = 0.1;
    double gamma = 0.1;
  };

  FtrlOptimizer(const base::FilePath& path,
                const Params& params,
                std::vector<std::unique_ptr<FtrlExpert>>&& experts);
  ~FtrlOptimizer();

  FtrlOptimizer(const FtrlOptimizer&) = delete;
  FtrlOptimizer& operator=(const FtrlOptimizer&) = delete;

  std::vector<double> Score(const std::vector<std::string>& items);

  void Train(const std::string& item);

 private:
  double Loss(size_t expert, const std::string& item);

  void OnProtoRead(ReadStatus status);

  Params params_;

  std::vector<std::unique_ptr<FtrlExpert>> experts_;

  // The items most recently passed to |Score|.
  std::vector<std::string> last_items_;
  // For each expert (outer vector) the scores returned by that expert for the
  // content of |last_items_|. The inner vector will be the same size as
  // |last_items_|.
  std::vector<std::vector<double>> last_expert_scores_;

  PersistentProto<FtrlOptimizerProto> proto_;

  base::WeakPtrFactory<FtrlOptimizer> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_
