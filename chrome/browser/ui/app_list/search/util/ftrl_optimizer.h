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
// TODO(crbug.com/1199206): Work in progress, expand this comment with algorithm
// details once implemented.
class FtrlOptimizer {
 public:
  // All user-settable parameters of the FTRL optimizer. The defaults should be
  // customized as-needed.
  struct Params {
    // How long to wait until writing any updates to disk.
    base::TimeDelta write_delay = base::Seconds(5);
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
  void OnProtoRead(ReadStatus status);

  Params params_;

  std::vector<std::unique_ptr<FtrlExpert>> experts_;

  PersistentProto<FtrlOptimizerProto> proto_;

  base::WeakPtrFactory<FtrlOptimizer> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_
